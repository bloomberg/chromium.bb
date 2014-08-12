// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/guest_view_base.h"

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/guest_view_constants.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using content::WebContents;

namespace extensions {

namespace {

typedef std::map<std::string, GuestViewBase::GuestCreationCallback>
    GuestViewCreationMap;
static base::LazyInstance<GuestViewCreationMap> guest_view_registry =
    LAZY_INSTANCE_INITIALIZER;

typedef std::map<WebContents*, GuestViewBase*> WebContentsGuestViewMap;
static base::LazyInstance<WebContentsGuestViewMap> webcontents_guestview_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

GuestViewBase::Event::Event(const std::string& name,
                            scoped_ptr<base::DictionaryValue> args)
    : name_(name), args_(args.Pass()) {
}

GuestViewBase::Event::~Event() {
}

scoped_ptr<base::DictionaryValue> GuestViewBase::Event::GetArguments() {
  return args_.Pass();
}

// This observer ensures that the GuestViewBase destroys itself when its
// embedder goes away.
class GuestViewBase::EmbedderWebContentsObserver : public WebContentsObserver {
 public:
  explicit EmbedderWebContentsObserver(GuestViewBase* guest)
      : WebContentsObserver(guest->embedder_web_contents()),
        destroyed_(false),
        guest_(guest) {
  }

  virtual ~EmbedderWebContentsObserver() {
  }

  // WebContentsObserver implementation.
  virtual void WebContentsDestroyed() OVERRIDE {
    Destroy();
  }

  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE {
    Destroy();
  }

 private:
  bool destroyed_;
  GuestViewBase* guest_;

  void Destroy() {
    if (destroyed_)
      return;
    destroyed_ = true;
    guest_->embedder_web_contents_ = NULL;
    guest_->EmbedderDestroyed();
    guest_->Destroy();
  }

  DISALLOW_COPY_AND_ASSIGN(EmbedderWebContentsObserver);
};

GuestViewBase::GuestViewBase(content::BrowserContext* browser_context,
                             int guest_instance_id)
    : embedder_web_contents_(NULL),
      embedder_render_process_id_(0),
      browser_context_(browser_context),
      guest_instance_id_(guest_instance_id),
      view_instance_id_(guestview::kInstanceIDNone),
      initialized_(false),
      auto_size_enabled_(false),
      weak_ptr_factory_(this) {
}

void GuestViewBase::Init(const std::string& embedder_extension_id,
                         content::WebContents* embedder_web_contents,
                         const base::DictionaryValue& create_params,
                         const WebContentsCreatedCallback& callback) {
  if (initialized_)
    return;
  initialized_ = true;

  extensions::Feature* feature =
      extensions::FeatureProvider::GetAPIFeatures()->GetFeature(
          GetAPINamespace());
  CHECK(feature);

  extensions::ProcessMap* process_map =
      extensions::ProcessMap::Get(browser_context());
  CHECK(process_map);

  const extensions::Extension* embedder_extension =
      extensions::ExtensionRegistry::Get(browser_context_)
          ->enabled_extensions()
          .GetByID(embedder_extension_id);
  // Ok for |embedder_extension| to be NULL, the embedder might be WebUI.

  CHECK(embedder_web_contents);
  int embedder_process_id =
      embedder_web_contents->GetRenderProcessHost()->GetID();

  extensions::Feature::Availability availability =
      feature->IsAvailableToContext(
          embedder_extension,
          process_map->GetMostLikelyContextType(embedder_extension,
                                                embedder_process_id),
          embedder_web_contents->GetLastCommittedURL());
  if (!availability.is_available()) {
    callback.Run(NULL);
    return;
  }

  CreateWebContents(embedder_extension_id,
                    embedder_process_id,
                    create_params,
                    base::Bind(&GuestViewBase::CompleteInit,
                               AsWeakPtr(),
                               embedder_extension_id,
                               embedder_process_id,
                               callback));
}

void GuestViewBase::InitWithWebContents(
    const std::string& embedder_extension_id,
    int embedder_render_process_id,
    content::WebContents* guest_web_contents) {
  DCHECK(guest_web_contents);
  content::RenderProcessHost* embedder_render_process_host =
      content::RenderProcessHost::FromID(embedder_render_process_id);

  embedder_extension_id_ = embedder_extension_id;
  embedder_render_process_id_ = embedder_render_process_host->GetID();
  embedder_render_process_host->AddObserver(this);

  WebContentsObserver::Observe(guest_web_contents);
  guest_web_contents->SetDelegate(this);
  webcontents_guestview_map.Get().insert(
      std::make_pair(guest_web_contents, this));
  GuestViewManager::FromBrowserContext(browser_context_)->
      AddGuest(guest_instance_id_, guest_web_contents);

  // Give the derived class an opportunity to perform additional initialization.
  DidInitialize();
}

void GuestViewBase::SetAutoSize(bool enabled,
                                const gfx::Size& min_size,
                                const gfx::Size& max_size) {
  min_auto_size_ = min_size;
  min_auto_size_.SetToMin(max_size);
  max_auto_size_ = max_size;
  max_auto_size_.SetToMax(min_size);

  enabled &= !min_auto_size_.IsEmpty() && !max_auto_size_.IsEmpty() &&
      IsAutoSizeSupported();
  if (!enabled && !auto_size_enabled_)
    return;

  auto_size_enabled_ = enabled;

  if (!attached())
    return;

  content::RenderViewHost* rvh = guest_web_contents()->GetRenderViewHost();
  if (auto_size_enabled_) {
    rvh->EnableAutoResize(min_auto_size_, max_auto_size_);
  } else {
    rvh->DisableAutoResize(element_size_);
    guest_size_ = element_size_;
    GuestSizeChangedDueToAutoSize(guest_size_, element_size_);
  }
}

// static
void GuestViewBase::RegisterGuestViewType(
    const std::string& view_type,
    const GuestCreationCallback& callback) {
  GuestViewCreationMap::iterator it =
      guest_view_registry.Get().find(view_type);
  DCHECK(it == guest_view_registry.Get().end());
  guest_view_registry.Get()[view_type] = callback;
}

// static
GuestViewBase* GuestViewBase::Create(
    content::BrowserContext* browser_context,
    int guest_instance_id,
    const std::string& view_type) {
  if (guest_view_registry.Get().empty())
    RegisterGuestViewTypes();

  GuestViewCreationMap::iterator it =
      guest_view_registry.Get().find(view_type);
  if (it == guest_view_registry.Get().end()) {
    NOTREACHED();
    return NULL;
  }
  return it->second.Run(browser_context, guest_instance_id);
}

// static
GuestViewBase* GuestViewBase::FromWebContents(WebContents* web_contents) {
  WebContentsGuestViewMap* guest_map = webcontents_guestview_map.Pointer();
  WebContentsGuestViewMap::iterator it = guest_map->find(web_contents);
  return it == guest_map->end() ? NULL : it->second;
}

// static
GuestViewBase* GuestViewBase::From(int embedder_process_id,
                                   int guest_instance_id) {
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(embedder_process_id);
  if (!host)
    return NULL;

  content::WebContents* guest_web_contents =
      GuestViewManager::FromBrowserContext(host->GetBrowserContext())->
          GetGuestByInstanceIDSafely(guest_instance_id, embedder_process_id);
  if (!guest_web_contents)
    return NULL;

  return GuestViewBase::FromWebContents(guest_web_contents);
}

// static
bool GuestViewBase::IsGuest(WebContents* web_contents) {
  return !!GuestViewBase::FromWebContents(web_contents);
}

base::WeakPtr<GuestViewBase> GuestViewBase::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool GuestViewBase::IsAutoSizeSupported() const {
  return false;
}

bool GuestViewBase::IsDragAndDropEnabled() const {
  return false;
}

void GuestViewBase::RenderProcessExited(content::RenderProcessHost* host,
                                        base::ProcessHandle handle,
                                        base::TerminationStatus status,
                                        int exit_code) {
  // GuestViewBase tracks the lifetime of its embedder render process until it
  // is attached to a particular embedder WebContents. At that point, its
  // lifetime is restricted in scope to the lifetime of its embedder
  // WebContents.
  CHECK(!attached());
  CHECK_EQ(host->GetID(), embedder_render_process_id());

  // This code path may be reached if the embedder WebContents is killed for
  // whatever reason immediately after a called to GuestViewInternal.createGuest
  // and before attaching the new guest to a frame.
  Destroy();
}

void GuestViewBase::Destroy() {
  DCHECK(guest_web_contents());
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(embedder_render_process_id());
  if (host)
    host->RemoveObserver(this);
  WillDestroy();
  if (!destruction_callback_.is_null())
    destruction_callback_.Run();

  webcontents_guestview_map.Get().erase(guest_web_contents());
  GuestViewManager::FromBrowserContext(browser_context_)->
      RemoveGuest(guest_instance_id_);
  pending_events_.clear();

  delete guest_web_contents();
}

void GuestViewBase::DidAttach() {
  // Give the derived class an opportunity to perform some actions.
  DidAttachToEmbedder();

  SendQueuedEvents();
}

void GuestViewBase::ElementSizeChanged(const gfx::Size& old_size,
                                       const gfx::Size& new_size) {
  element_size_ = new_size;
}

int GuestViewBase::GetGuestInstanceID() const {
  return guest_instance_id_;
}

void GuestViewBase::GuestSizeChanged(const gfx::Size& old_size,
                                     const gfx::Size& new_size) {
  if (!auto_size_enabled_)
    return;
  guest_size_ = new_size;
  GuestSizeChangedDueToAutoSize(old_size, new_size);
}

void GuestViewBase::SetOpener(GuestViewBase* guest) {
  if (guest && guest->IsViewType(GetViewType())) {
    opener_ = guest->AsWeakPtr();
    return;
  }
  opener_ = base::WeakPtr<GuestViewBase>();
}

void GuestViewBase::RegisterDestructionCallback(
    const DestructionCallback& callback) {
  destruction_callback_ = callback;
}

void GuestViewBase::WillAttach(content::WebContents* embedder_web_contents,
                               const base::DictionaryValue& extra_params) {
  // After attachment, this GuestViewBase's lifetime is restricted to the
  // lifetime of its embedder WebContents. Observing the RenderProcessHost
  // of the embedder is no longer necessary.
  embedder_web_contents->GetRenderProcessHost()->RemoveObserver(this);
  embedder_web_contents_ = embedder_web_contents;
  embedder_web_contents_observer_.reset(
      new EmbedderWebContentsObserver(this));
  extra_params.GetInteger(guestview::kParameterInstanceId, &view_instance_id_);
  extra_params_.reset(extra_params.DeepCopy());

  WillAttachToEmbedder();
}

void GuestViewBase::DidStopLoading(content::RenderViewHost* render_view_host) {
  if (!IsDragAndDropEnabled()) {
    const char script[] = "window.addEventListener('dragstart', function() { "
                          "  window.event.preventDefault(); "
                          "});";
    render_view_host->GetMainFrame()->ExecuteJavaScript(
        base::ASCIIToUTF16(script));
  }
  DidStopLoading();
}

void GuestViewBase::RenderViewReady() {
  GuestReady();
  content::RenderViewHost* rvh = guest_web_contents()->GetRenderViewHost();
  if (auto_size_enabled_) {
    rvh->EnableAutoResize(min_auto_size_, max_auto_size_);
  } else {
    rvh->DisableAutoResize(element_size_);
  }
}

void GuestViewBase::WebContentsDestroyed() {
  GuestDestroyed();
  delete this;
}

bool GuestViewBase::ShouldFocusPageAfterCrash() {
  // Focus is managed elsewhere.
  return false;
}

bool GuestViewBase::PreHandleGestureEvent(content::WebContents* source,
                                         const blink::WebGestureEvent& event) {
  return event.type == blink::WebGestureEvent::GesturePinchBegin ||
      event.type == blink::WebGestureEvent::GesturePinchUpdate ||
      event.type == blink::WebGestureEvent::GesturePinchEnd;
}

GuestViewBase::~GuestViewBase() {
}

void GuestViewBase::DispatchEventToEmbedder(Event* event) {
  scoped_ptr<Event> event_ptr(event);
  if (!in_extension()) {
    NOTREACHED();
    return;
  }

  if (!attached()) {
    pending_events_.push_back(linked_ptr<Event>(event_ptr.release()));
    return;
  }

  extensions::EventFilteringInfo info;
  info.SetInstanceID(view_instance_id_);
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(event->GetArguments().release());

  extensions::EventRouter::DispatchEvent(
      embedder_web_contents_,
      browser_context_,
      embedder_extension_id_,
      event->name(),
      args.Pass(),
      extensions::EventRouter::USER_GESTURE_UNKNOWN,
      info);
}

void GuestViewBase::SendQueuedEvents() {
  if (!attached())
    return;
  while (!pending_events_.empty()) {
    linked_ptr<Event> event_ptr = pending_events_.front();
    pending_events_.pop_front();
    DispatchEventToEmbedder(event_ptr.release());
  }
}

void GuestViewBase::CompleteInit(const std::string& embedder_extension_id,
                                 int embedder_render_process_id,
                                 const WebContentsCreatedCallback& callback,
                                 content::WebContents* guest_web_contents) {
  if (!guest_web_contents) {
    // The derived class did not create a WebContents so this class serves no
    // purpose. Let's self-destruct.
    delete this;
    callback.Run(NULL);
    return;
  }
  InitWithWebContents(embedder_extension_id,
                      embedder_render_process_id,
                      guest_web_contents);
  callback.Run(guest_web_contents);
}

// static
void GuestViewBase::RegisterGuestViewTypes() {
  extensions::ExtensionsAPIClient::Get()->RegisterGuestViewTypes();
}

}  // namespace extensions
