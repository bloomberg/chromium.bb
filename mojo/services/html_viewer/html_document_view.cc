// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/html_document_view.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/services/html_viewer/blink_input_events_type_converters.h"
#include "mojo/services/html_viewer/blink_url_request_type_converters.h"
#include "mojo/services/html_viewer/webstoragenamespace_impl.h"
#include "mojo/services/html_viewer/weburlloader_impl.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace mojo {
namespace {

void ConfigureSettings(blink::WebSettings* settings) {
  settings->setAcceleratedCompositingEnabled(false);
  settings->setCookieEnabled(true);
  settings->setDefaultFixedFontSize(13);
  settings->setDefaultFontSize(16);
  settings->setLoadsImagesAutomatically(true);
  settings->setJavaScriptEnabled(true);
}

Target WebNavigationPolicyToNavigationTarget(
    blink::WebNavigationPolicy policy) {
  switch (policy) {
    case blink::WebNavigationPolicyCurrentTab:
      return TARGET_SOURCE_NODE;
    case blink::WebNavigationPolicyNewBackgroundTab:
    case blink::WebNavigationPolicyNewForegroundTab:
    case blink::WebNavigationPolicyNewWindow:
    case blink::WebNavigationPolicyNewPopup:
      return TARGET_NEW_NODE;
    default:
      return TARGET_DEFAULT;
  }
}

bool CanNavigateLocally(blink::WebFrame* frame,
                        const blink::WebURLRequest& request) {
  // For now, we just load child frames locally.
  // TODO(aa): In the future, this should use embedding to connect to a
  // different instance of Blink if the frame is cross-origin.
  if (frame->parent())
    return true;

  // If we have extraData() it means we already have the url response
  // (presumably because we are being called via Navigate()). In that case we
  // can go ahead and navigate locally.
  if (request.extraData())
    return true;

  // Otherwise we don't know if we're the right app to handle this request. Ask
  // host to do the navigation for us.
  return false;
}

}  // namespace

HTMLDocumentView::HTMLDocumentView(
    URLResponsePtr response,
    scoped_ptr<ServiceProvider> imported_services,
    ServiceProviderImpl* exported_services,
    Shell* shell)
    : imported_services_(imported_services.Pass()),
      shell_(shell),
      web_view_(NULL),
      root_(NULL),
      view_manager_client_factory_(shell, this),
      repaint_pending_(false),
      weak_factory_(this) {
  exported_services->AddService(&view_manager_client_factory_);
  Load(response.Pass());
}

HTMLDocumentView::~HTMLDocumentView() {
  if (web_view_)
    web_view_->close();
  if (root_)
    root_->RemoveObserver(this);
}

void HTMLDocumentView::OnEmbed(ViewManager* view_manager,
                               View* root,
                               ServiceProviderImpl* exported_services,
                               scoped_ptr<ServiceProvider> imported_services) {
  root_ = root;
  root_->SetColor(SK_ColorCYAN);  // Dummy background color.
  web_view_->resize(root_->bounds().size());
  root_->AddObserver(this);
}

void HTMLDocumentView::OnViewManagerDisconnected(ViewManager* view_manager) {
  // TODO(aa): Need to figure out how shutdown works.
}

void HTMLDocumentView::Load(URLResponsePtr response) {
  web_view_ = blink::WebView::create(this);
  ConfigureSettings(web_view_->settings());
  web_view_->setMainFrame(blink::WebLocalFrame::create(this));

  GURL url(response->url);

  WebURLRequestExtraData* extra_data = new WebURLRequestExtraData;
  extra_data->synthetic_response = response.Pass();

  blink::WebURLRequest web_request;
  web_request.initialize();
  web_request.setURL(url);
  web_request.setExtraData(extra_data);

  web_view_->mainFrame()->loadRequest(web_request);
}

blink::WebStorageNamespace* HTMLDocumentView::createSessionStorageNamespace() {
  return new WebStorageNamespaceImpl();
}

void HTMLDocumentView::didInvalidateRect(const blink::WebRect& rect) {
  if (!repaint_pending_) {
    repaint_pending_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&HTMLDocumentView::Repaint, weak_factory_.GetWeakPtr()));
  }
}

bool HTMLDocumentView::allowsBrokenNullLayerTreeView() const {
  // TODO(darin): Switch to using compositor bindings.
  //
  // NOTE: Note to Blink maintainers, feel free to break this code if it is the
  // last NOT using compositor bindings and you want to delete this code path.
  //
  return true;
}

blink::WebFrame* HTMLDocumentView::createChildFrame(
    blink::WebLocalFrame* parent,
    const blink::WebString& frameName) {
  blink::WebLocalFrame* web_frame = blink::WebLocalFrame::create(this);
  parent->appendChild(web_frame);
  return web_frame;
}

void HTMLDocumentView::frameDetached(blink::WebFrame* frame) {
  if (frame->parent())
    frame->parent()->removeChild(frame);

  // |frame| is invalid after here.
  frame->close();
}

blink::WebCookieJar* HTMLDocumentView::cookieJar(blink::WebLocalFrame* frame) {
  // TODO(darin): Blink does not fallback to the Platform provided WebCookieJar.
  // Either it should, as it once did, or we should find another solution here.
  return blink::Platform::current()->cookieJar();
}

blink::WebNavigationPolicy HTMLDocumentView::decidePolicyForNavigation(
    blink::WebLocalFrame* frame, blink::WebDataSource::ExtraData* data,
    const blink::WebURLRequest& request, blink::WebNavigationType nav_type,
    blink::WebNavigationPolicy default_policy, bool is_redirect) {
  if (CanNavigateLocally(frame, request))
    return default_policy;

  NavigationDetailsPtr nav_details(NavigationDetails::New());
  nav_details->request = URLRequest::From(request);

  GetNavigatorHost()->RequestNavigate(
      root_->id(),
      WebNavigationPolicyToNavigationTarget(default_policy),
      nav_details.Pass());

  return blink::WebNavigationPolicyIgnore;
}

void HTMLDocumentView::didAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
}

void HTMLDocumentView::didNavigateWithinPage(
    blink::WebLocalFrame* frame, const blink::WebHistoryItem& history_item,
    blink::WebHistoryCommitType commit_type) {
  GetNavigatorHost()->DidNavigateLocally(root_->id(),
                                         history_item.urlString().utf8());
}

void HTMLDocumentView::OnViewBoundsChanged(View* view,
                                           const gfx::Rect& old_bounds,
                                           const gfx::Rect& new_bounds) {
  DCHECK_EQ(view, root_);
  web_view_->resize(view->bounds().size());
}

void HTMLDocumentView::OnViewDestroyed(View* view) {
  DCHECK_EQ(view, root_);
  view->RemoveObserver(this);
  root_ = NULL;
}

void HTMLDocumentView::OnViewInputEvent(View* view, const EventPtr& event) {
  scoped_ptr<blink::WebInputEvent> web_event =
      event.To<scoped_ptr<blink::WebInputEvent> >();
  if (web_event)
    web_view_->handleInputEvent(*web_event);
}

void HTMLDocumentView::Repaint() {
  repaint_pending_ = false;

  if (!root_)
    return;

  web_view_->animate(0.0);
  web_view_->layout();

  int width = web_view_->size().width;
  int height = web_view_->size().height;

  skia::RefPtr<SkCanvas> canvas = skia::AdoptRef(SkCanvas::NewRaster(
      SkImageInfo::MakeN32(width, height, kOpaque_SkAlphaType)));

  web_view_->paint(canvas.get(), gfx::Rect(0, 0, width, height));

  root_->SetContents(canvas->getDevice()->accessBitmap(false));
}

NavigatorHost* HTMLDocumentView::GetNavigatorHost() {
  if (!navigator_host_.get()) {
    // TODO(aa): This should come via |imported_services| in OnEmbed().
    InterfacePtr<ServiceProvider> sp;
    shell_->ConnectToApplication("mojo:mojo_window_manager", Get(&sp));
    ConnectToService(sp.get(), &navigator_host_);
  }
  return navigator_host_.get();
}

}  // namespace mojo
