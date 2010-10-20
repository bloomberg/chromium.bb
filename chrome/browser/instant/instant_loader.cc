// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_loader.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/browser/history/history_marshaling.h"
#include "chrome/browser/instant/instant_loader_delegate.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/renderer_preferences.h"
#include "gfx/codec/png_codec.h"
#include "ipc/ipc_message.h"

namespace {

// Script sent as the user is typing and the provider supports instant.
// Params:
// . the text the user typed.
// TODO: add support for the 2nd and 3rd params.
const char kUserInputScript[] =
    "if (window.chrome.userInput) window.chrome.userInput(\"$1\", 0, 0);";

// Script sent when the page is committed and the provider supports instant.
// Params:
// . the text the user typed.
// . boolean indicating if the user pressed enter to accept the text.
const char kUserDoneScript[] =
    "if (window.chrome.userWantsQuery) "
      "window.chrome.userWantsQuery(\"$1\", $2);";

// Script sent when the bounds of the omnibox changes and the provider supports
// instant. The params are the bounds relative to the origin of the preview
// (x, y, width, height).
const char kSetOmniboxBoundsScript[] =
    "if (window.chrome.setDropdownDimensions) "
    "window.chrome.setDropdownDimensions($1, $2, $3, $4);";

// Script sent to see if the page really supports instant.
const char kSupportsInstantScript[] =
    "if (window.chrome.setDropdownDimensions) true; else false;";

// Number of ms to delay before updating the omnibox bounds. This is a bit long
// as updating the bounds ends up being quite expensive.
const int kUpdateBoundsDelayMS = 500;

// Escapes quotes in the |text| so that it be passed to JavaScript as a quoted
// string.
string16 EscapeUserText(const string16& text) {
  string16 escaped_text(text);
  ReplaceSubstringsAfterOffset(&escaped_text, 0L, ASCIIToUTF16("\""),
                               ASCIIToUTF16("\\\""));
  return escaped_text;
}

// Sends the script for when the user commits the preview. |pressed_enter| is
// true if the user pressed enter to commit.
void SendDoneScript(TabContents* tab_contents,
                    const string16& text,
                    bool pressed_enter) {
  std::vector<string16> params;
  params.push_back(EscapeUserText(text));
  params.push_back(pressed_enter ? ASCIIToUTF16("true") :
                   ASCIIToUTF16("false"));
  string16 script = ReplaceStringPlaceholders(ASCIIToUTF16(kUserDoneScript),
                                              params,
                                              NULL);
  tab_contents->render_view_host()->ExecuteJavascriptInWebFrame(
      std::wstring(),
      UTF16ToWide(script));
}

// Sends the user input script to |tab_contents|. |text| is the text the user
// input into the omnibox.
void SendUserInputScript(TabContents* tab_contents, const string16& text) {
  string16 script = ReplaceStringPlaceholders(ASCIIToUTF16(kUserInputScript),
                                              EscapeUserText(text),
                                              NULL);
  tab_contents->render_view_host()->ExecuteJavascriptInWebFrame(
      std::wstring(),
      UTF16ToWide(script));
}

// Sends the script for setting the bounds of the omnibox to |tab_contents|.
void SendOmniboxBoundsScript(TabContents* tab_contents,
                             const gfx::Rect& bounds) {
  std::vector<string16> bounds_vector;
  bounds_vector.push_back(base::IntToString16(bounds.x()));
  bounds_vector.push_back(base::IntToString16(bounds.y()));
  bounds_vector.push_back(base::IntToString16(bounds.width()));
  bounds_vector.push_back(base::IntToString16(bounds.height()));
  string16 script = ReplaceStringPlaceholders(
      ASCIIToUTF16(kSetOmniboxBoundsScript),
      bounds_vector,
      NULL);
  tab_contents->render_view_host()->ExecuteJavascriptInWebFrame(
      std::wstring(),
      UTF16ToWide(script));
}

}  // namespace

// FrameLoadObserver is responsible for waiting for the TabContents to finish
// loading and when done sending the necessary script down to the page.
class InstantLoader::FrameLoadObserver : public NotificationObserver {
 public:
  FrameLoadObserver(InstantLoader* loader, const string16& text)
      : loader_(loader),
        tab_contents_(loader->preview_contents()),
        unique_id_(tab_contents_->controller().pending_entry()->unique_id()),
        text_(text),
        initial_text_(text),
        execute_js_id_(0) {
    registrar_.Add(this, NotificationType::LOAD_COMPLETED_MAIN_FRAME,
                   Source<TabContents>(tab_contents_));
  }

  // Sets the text to send to the page.
  void set_text(const string16& text) { text_ = text; }

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::LOAD_COMPLETED_MAIN_FRAME: {
        int page_id = *(Details<int>(details).ptr());
        NavigationEntry* active_entry =
            tab_contents_->controller().GetActiveEntry();
        if (!active_entry || active_entry->page_id() != page_id ||
            active_entry->unique_id() != unique_id_) {
          return;
        }

        DetermineIfPageSupportsInstant();
        break;
      }

      case NotificationType::EXECUTE_JAVASCRIPT_RESULT: {
        typedef std::pair<int, Value*> ExecuteDetailType;
        ExecuteDetailType* result =
            (static_cast<Details<ExecuteDetailType > >(details)).ptr();
        if (result->first != execute_js_id_)
          return;

        DCHECK(loader_);
        bool bool_result;
        if (!result->second || !result->second->IsType(Value::TYPE_BOOLEAN) ||
            !result->second->GetAsBoolean(&bool_result) || !bool_result) {
          DoesntSupportInstant();
          return;
        }
        SupportsInstant();
        return;
      }

      default:
        NOTREACHED();
        break;
    }
  }

 private:
  // Executes the javascript to determine if the page supports script.  The
  // results are passed back to us by way of NotificationObserver.
  void DetermineIfPageSupportsInstant() {
    DCHECK_EQ(0, execute_js_id_);

    RenderViewHost* rvh = tab_contents_->render_view_host();
    registrar_.Add(this, NotificationType::EXECUTE_JAVASCRIPT_RESULT,
                   Source<RenderViewHost>(rvh));
    execute_js_id_ = rvh->ExecuteJavascriptInWebFrameNotifyResult(
        string16(),
        ASCIIToUTF16(kSupportsInstantScript));
  }

  // Invoked when we determine the page doesn't really support instant.
  void DoesntSupportInstant() {
    DCHECK(loader_);

    loader_->PageDoesntSupportInstant(text_ != initial_text_);

    // WARNING: we've been deleted.
  }

  // Invoked when we determine the page really supports instant.
  void SupportsInstant() {
    DCHECK(loader_);

    gfx::Rect bounds = loader_->GetOmniboxBoundsInTermsOfPreview();
    loader_->last_omnibox_bounds_ = loader_->omnibox_bounds_;
    if (!bounds.IsEmpty())
      SendOmniboxBoundsScript(tab_contents_, bounds);

    SendUserInputScript(tab_contents_, text_);

    loader_->PageFinishedLoading();

    // WARNING: we've been deleted.
  }

  // InstantLoader that created us.
  InstantLoader* loader_;

  // The TabContents we're listening for changes on.
  TabContents* tab_contents_;

  // unique_id of the NavigationEntry we're waiting on.
  const int unique_id_;

  // Text to send down to the page.
  string16 text_;

  // Initial text supplied to constructor.
  const string16 initial_text_;

  // Registers and unregisters us for notifications.
  NotificationRegistrar registrar_;

  // ID of the javascript that was executed to determine if the page supports
  // instant.
  int execute_js_id_;

  DISALLOW_COPY_AND_ASSIGN(FrameLoadObserver);
};

// PaintObserver implementation. When the RenderWidgetHost paints itself this
// notifies InstantLoader, which makes the TabContents active.
class InstantLoader::PaintObserverImpl
    : public RenderWidgetHost::PaintObserver {
 public:
  explicit PaintObserverImpl(InstantLoader* loader) : loader_(loader) {
  }

  virtual void RenderWidgetHostWillPaint(RenderWidgetHost* rwh) {
  }

  virtual void RenderWidgetHostDidPaint(RenderWidgetHost* rwh) {
    loader_->PreviewPainted();
    rwh->set_paint_observer(NULL);
    // WARNING: we've been deleted.
  }

 private:
  InstantLoader* loader_;

  DISALLOW_COPY_AND_ASSIGN(PaintObserverImpl);
};

class InstantLoader::TabContentsDelegateImpl : public TabContentsDelegate {
 public:
  explicit TabContentsDelegateImpl(InstantLoader* loader)
      : loader_(loader),
        installed_paint_observer_(false),
        waiting_for_new_page_(true),
        is_mouse_down_from_activate_(false) {
  }

  // Invoked prior to loading a new URL.
  void PrepareForNewLoad() {
    waiting_for_new_page_ = true;
    add_page_vector_.clear();
  }

  // Invoked when removed as the delegate. Gives a chance to do any necessary
  // cleanup.
  void Reset() {
    installed_paint_observer_ = false;
    is_mouse_down_from_activate_ = false;
  }

  bool is_mouse_down_from_activate() const {
    return is_mouse_down_from_activate_;
  }

  // Commits the currently buffered history.
  void CommitHistory() {
    TabContents* tab = loader_->preview_contents();
    if (tab->profile()->IsOffTheRecord())
      return;

    for (size_t i = 0; i < add_page_vector_.size(); ++i)
      tab->UpdateHistoryForNavigation(add_page_vector_[i].get());

    NavigationEntry* active_entry = tab->controller().GetActiveEntry();
    DCHECK(active_entry);
    tab->UpdateHistoryPageTitle(*active_entry);

    FaviconService* favicon_service =
        tab->profile()->GetFaviconService(Profile::EXPLICIT_ACCESS);

    if (favicon_service && active_entry->favicon().is_valid() &&
        !active_entry->favicon().bitmap().empty()) {
      std::vector<unsigned char> image_data;
      gfx::PNGCodec::EncodeBGRASkBitmap(active_entry->favicon().bitmap(), false,
                                        &image_data);
      favicon_service->SetFavicon(active_entry->url(),
                                  active_entry->favicon().url(),
                                  image_data);
    }
  }

  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition) {}
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {
    if (!installed_paint_observer_ && source->controller().entry_count()) {
      // The load has been committed. Install an observer that waits for the
      // first paint then makes the preview active. We wait for the load to be
      // committed before waiting on paint as there is always an initial paint
      // when a new renderer is created from the resize so that if we showed the
      // preview after the first paint we would end up with a white rect.
      installed_paint_observer_ = true;
      source->GetRenderWidgetHostView()->GetRenderWidgetHost()->
          set_paint_observer(new PaintObserverImpl(loader_));
    }
  }
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) {}
  virtual void ActivateContents(TabContents* contents) {
  }
  virtual void DeactivateContents(TabContents* contents) {}
  virtual void LoadingStateChanged(TabContents* source) {}
  virtual void CloseContents(TabContents* source) {}
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual void DetachContents(TabContents* source) {}
  virtual bool IsPopup(const TabContents* source) const {
    return false;
  }
  virtual bool ShouldFocusConstrainedWindow(TabContents* source) {
    // Return false so that constrained windows are not initially focused. If
    // we did otherwise the preview would prematurely get committed when focus
    // goes to the constrained window.
    return false;
  }
  virtual void WillShowConstrainedWindow(TabContents* source) {
    if (!loader_->ready()) {
      // A constrained window shown for an auth may not paint. Show the preview
      // contents.
      if (installed_paint_observer_) {
        source->GetRenderWidgetHostView()->GetRenderWidgetHost()->
            set_paint_observer(NULL);
      }
      installed_paint_observer_ = true;
      loader_->ShowPreview();
    }
  }
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
  virtual void URLStarredChanged(TabContents* source, bool starred) {}
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}
  virtual void ContentsMouseEvent(
      TabContents* source, const gfx::Point& location, bool motion) {}
  virtual void ContentsZoomChange(bool zoom_in) {}
  virtual void OnContentSettingsChange(TabContents* source) {}
  virtual bool IsApplication() const { return false; }
  virtual void ConvertContentsToApplication(TabContents* source) {}
  virtual bool CanReloadContents(TabContents* source) const { return true; }
  virtual gfx::Rect GetRootWindowResizerRect() const {
    return gfx::Rect();
  }
  virtual void ShowHtmlDialog(HtmlDialogUIDelegate* delegate,
                              gfx::NativeWindow parent_window) {}
  virtual void BeforeUnloadFired(TabContents* tab,
                                 bool proceed,
                                 bool* proceed_to_fire_unload) {}
  virtual void ForwardMessageToExternalHost(const std::string& message,
                                            const std::string& origin,
                                            const std::string& target) {}
  virtual bool IsExternalTabContainer() const { return false; }
  virtual void SetFocusToLocationBar(bool select_all) {}
  virtual bool ShouldFocusPageAfterCrash() { return false; }
  virtual void RenderWidgetShowing() {}
  virtual ExtensionFunctionDispatcher* CreateExtensionFunctionDispatcher(
      RenderViewHost* render_view_host,
      const std::string& extension_id) {
    return NULL;
  }
  virtual bool TakeFocus(bool reverse) { return false; }
  virtual void LostCapture() {
    CommitFromMouseReleaseIfNecessary();
  }
  virtual void SetTabContentBlocked(TabContents* contents, bool blocked) {}
  virtual void TabContentsFocused(TabContents* tab_content) {
  }
  virtual int GetExtraRenderViewHeight() const { return 0; }
  virtual bool CanDownload(int request_id) { return false; }
  virtual void OnStartDownload(DownloadItem* download, TabContents* tab) {}
  virtual bool HandleContextMenu(const ContextMenuParams& params) {
    return false;
  }
  virtual bool ExecuteContextMenuCommand(int command) {
    return false;
  }
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                        Profile* profile) {}
  virtual void ShowPageInfo(Profile* profile,
                            const GURL& url,
                            const NavigationEntry::SSLStatus& ssl,
                            bool show_history) {}
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) {
    return false;
  }
  virtual void HandleMouseUp() {
    CommitFromMouseReleaseIfNecessary();
  }
  virtual void HandleMouseActivate() {
    is_mouse_down_from_activate_ = true;
  }
  virtual void ShowRepostFormWarningDialog(TabContents* tab_contents) {}
  virtual void ShowContentSettingsWindow(ContentSettingsType content_type) {}
  virtual void ShowCollectedCookiesDialog(TabContents* tab_contents) {}
  virtual bool OnGoToEntryOffset(int offset) { return false; }
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      NavigationType::Type navigation_type) {
    if (waiting_for_new_page_ && navigation_type == NavigationType::NEW_PAGE)
      waiting_for_new_page_ = false;

    if (!waiting_for_new_page_) {
      add_page_vector_.push_back(
          scoped_refptr<history::HistoryAddPageArgs>(add_page_args.Clone()));
    }
    return false;
  }
  virtual void OnDidGetApplicationInfo(TabContents* tab_contents,
                                       int32 page_id) {}
  virtual gfx::NativeWindow GetFrameNativeWindow() {
    return NULL;
  }
  virtual void TabContentsCreated(TabContents* new_contents) {}
  virtual bool infobars_enabled() { return false; }
  virtual bool ShouldEnablePreferredSizeNotifications() { return false; }
  virtual void UpdatePreferredSize(const gfx::Size& pref_size) {}
  virtual void ContentTypeChanged(TabContents* source) {}

  virtual void OnSetSuggestResult(int32 page_id, const std::string& result) {
    TabContents* source = loader_->preview_contents();
    // TODO: only allow for default search provider.
    if (source->controller().GetActiveEntry() &&
        page_id == source->controller().GetActiveEntry()->page_id()) {
      loader_->SetCompleteSuggestedText(UTF8ToUTF16(result));
    }
  }

 private:
  typedef std::vector<scoped_refptr<history::HistoryAddPageArgs> >
      AddPageVector;

  void CommitFromMouseReleaseIfNecessary() {
    bool was_down = is_mouse_down_from_activate_;
    is_mouse_down_from_activate_ = false;
    if (was_down && loader_->ShouldCommitInstantOnMouseUp())
      loader_->CommitInstantLoader();
  }

  InstantLoader* loader_;

  // Has the paint observer been installed? See comment in
  // NavigationStateChanged for details on this.
  bool installed_paint_observer_;

  // Used to cache data that needs to be added to history. Normally entries are
  // added to history as the user types, but for instant we only want to add the
  // items to history if the user commits instant. So, we cache them here and if
  // committed then add the items to history.
  AddPageVector add_page_vector_;

  // Are we we waiting for a NavigationType of NEW_PAGE? If we're waiting for
  // NEW_PAGE navigation we don't add history items to add_page_vector_.
  bool waiting_for_new_page_;

  // Returns true if the mouse is down from an activate.
  bool is_mouse_down_from_activate_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsDelegateImpl);
};

InstantLoader::InstantLoader(InstantLoaderDelegate* delegate, TemplateURLID id)
    : delegate_(delegate),
      template_url_id_(id),
      ready_(false),
      last_transition_type_(PageTransition::LINK) {
  preview_tab_contents_delegate_.reset(new TabContentsDelegateImpl(this));
}

InstantLoader::~InstantLoader() {
  // Delete the TabContents before the delegate as the TabContents holds a
  // reference to the delegate.
  preview_contents_.reset(NULL);
}

void InstantLoader::Update(TabContents* tab_contents,
                           const TemplateURL* template_url,
                           const GURL& url,
                           PageTransition::Type transition_type,
                           const string16& user_text,
                           string16* suggested_text) {
  if (url_ == url)
    return;

  DCHECK(!url.is_empty() && url.is_valid());

  last_transition_type_ = transition_type;
  url_ = url;
  user_text_ = user_text;

  bool created_preview_contents;
  if (preview_contents_.get() == NULL) {
    preview_contents_.reset(
        new TabContents(tab_contents->profile(), NULL, MSG_ROUTING_NONE,
                        NULL, NULL));
    preview_contents_->SetAllContentsBlocked(true);
    // Propagate the max page id. That way if we end up merging the two
    // NavigationControllers (which happens if we commit) none of the page ids
    // will overlap.
    int32 max_page_id = tab_contents->GetMaxPageID();
    if (max_page_id != -1)
      preview_contents_->controller().set_max_restored_page_id(max_page_id + 1);

    preview_contents_->set_delegate(preview_tab_contents_delegate_.get());

    gfx::Rect tab_bounds;
    tab_contents->view()->GetContainerBounds(&tab_bounds);
    preview_contents_->view()->SizeContents(tab_bounds.size());

    preview_contents_->ShowContents();
    created_preview_contents = true;
  } else {
    created_preview_contents = false;
  }
  preview_tab_contents_delegate_->PrepareForNewLoad();

  if (template_url) {
    DCHECK(template_url_id_ == template_url->id());
    if (!created_preview_contents) {
      if (is_waiting_for_load()) {
        // The page hasn't loaded yet. We'll send the script down when it does.
        frame_load_observer_->set_text(user_text_);
        return;
      }
      SendUserInputScript(preview_contents_.get(), user_text_);
      if (complete_suggested_text_.size() > user_text_.size() &&
          !complete_suggested_text_.compare(0, user_text_.size(), user_text_)) {
        *suggested_text = complete_suggested_text_.substr(user_text_.size());
      }
    } else {
      // Load the instant URL. We don't reflect the url we load in url() as
      // callers expect that we're loading the URL they tell us to.
      GURL instant_url(
          template_url->instant_url()->ReplaceSearchTerms(
              *template_url, UTF16ToWideHack(user_text), -1, std::wstring()));
      initial_instant_url_ = url;
      preview_contents_->controller().LoadURL(
          instant_url, GURL(), transition_type);
      frame_load_observer_.reset(new FrameLoadObserver(this, user_text_));
    }
  } else {
    DCHECK(template_url_id_ == 0);
    frame_load_observer_.reset(NULL);
    preview_contents_->controller().LoadURL(url_, GURL(), transition_type);
  }
}

void InstantLoader::SetOmniboxBounds(const gfx::Rect& bounds) {
  if (omnibox_bounds_ == bounds)
    return;

  omnibox_bounds_ = bounds;
  if (preview_contents_.get() && is_showing_instant() &&
      !is_waiting_for_load()) {
    // Updating the bounds is rather expensive, and because of the async nature
    // of the omnibox the bounds can dance around a bit. Delay the update in
    // hopes of things settling down.
    if (update_bounds_timer_.IsRunning())
      update_bounds_timer_.Stop();
    update_bounds_timer_.Start(
        base::TimeDelta::FromMilliseconds(kUpdateBoundsDelayMS),
        this, &InstantLoader::ProcessBoundsChange);
  }
}

bool InstantLoader::IsMouseDownFromActivate() {
  return preview_tab_contents_delegate_->is_mouse_down_from_activate();
}

TabContents* InstantLoader::ReleasePreviewContents(InstantCommitType type) {
  if (!preview_contents_.get())
    return NULL;

  // FrameLoadObserver is only used for instant results, and instant results are
  // only committed if active (when the FrameLoadObserver isn't installed).
  DCHECK(type == INSTANT_COMMIT_DESTROY || !frame_load_observer_.get());

  if (type != INSTANT_COMMIT_DESTROY && is_showing_instant()) {
    SendDoneScript(preview_contents_.get(),
                   user_text_,
                   type == INSTANT_COMMIT_PRESSED_ENTER);
  }
  omnibox_bounds_ = gfx::Rect();
  last_omnibox_bounds_ = gfx::Rect();
  url_ = GURL();
  user_text_.clear();
  complete_suggested_text_.clear();
  if (preview_contents_.get()) {
    if (type != INSTANT_COMMIT_DESTROY)
      preview_tab_contents_delegate_->CommitHistory();
    // Destroy the paint observer.
    // RenderWidgetHostView may be null during shutdown.
    if (preview_contents_->GetRenderWidgetHostView()) {
      preview_contents_->GetRenderWidgetHostView()->GetRenderWidgetHost()->
          set_paint_observer(NULL);
#if defined(OS_MACOSX)
      preview_contents_->GetRenderWidgetHostView()->
          SetTakesFocusOnlyOnMouseDown(false);
#endif
    }
    preview_contents_->set_delegate(NULL);
    preview_tab_contents_delegate_->Reset();
    ready_ = false;
  }
  update_bounds_timer_.Stop();
  return preview_contents_.release();
}

bool InstantLoader::ShouldCommitInstantOnMouseUp() {
  return delegate_->ShouldCommitInstantOnMouseUp();
}

void InstantLoader::CommitInstantLoader() {
  delegate_->CommitInstantLoader(this);
}

void InstantLoader::ClearTemplateURLID() {
  // This should only be invoked for sites we thought supported instant.
  DCHECK(template_url_id_);

  // The frame load observer should have completed.
  DCHECK(!frame_load_observer_.get());

  // We shouldn't be ready.
  DCHECK(!ready());

  template_url_id_ = 0;
  ShowPreview();
}

void InstantLoader::SetCompleteSuggestedText(
    const string16& complete_suggested_text) {
  ShowPreview();

  if (complete_suggested_text == complete_suggested_text_)
    return;

  if (user_text_.compare(0, user_text_.size(), complete_suggested_text,
                         0, user_text_.size())) {
    // The user text no longer contains the suggested text, ignore it.
    complete_suggested_text_.clear();
    delegate_->SetSuggestedTextFor(this, string16());
    return;
  }

  complete_suggested_text_ = complete_suggested_text;
  delegate_->SetSuggestedTextFor(
      this,
      complete_suggested_text_.substr(user_text_.size()));
}

void InstantLoader::PreviewPainted() {
  // If instant is supported then we wait for the first suggest result before
  // showing the page.
  if (!template_url_id_)
    ShowPreview();
}

void InstantLoader::ShowPreview() {
  if (!ready_) {
    ready_ = true;
#if defined(OS_MACOSX)
    if (preview_contents_->GetRenderWidgetHostView()) {
      preview_contents_->GetRenderWidgetHostView()->
          SetTakesFocusOnlyOnMouseDown(true);
    }
#endif
    delegate_->ShowInstantLoader(this);
  }
}

void InstantLoader::PageFinishedLoading() {
  frame_load_observer_.reset();
  // Wait for the user input before showing, this way the page should be up to
  // date by the time we show it.
}

gfx::Rect InstantLoader::GetOmniboxBoundsInTermsOfPreview() {
  if (omnibox_bounds_.IsEmpty())
    return omnibox_bounds_;

  gfx::Rect preview_bounds(delegate_->GetInstantBounds());
  return gfx::Rect(omnibox_bounds_.x() - preview_bounds.x(),
                   omnibox_bounds_.y() - preview_bounds.y(),
                   omnibox_bounds_.width(),
                   omnibox_bounds_.height());
}

void InstantLoader::PageDoesntSupportInstant(bool needs_reload) {
  GURL url_to_load = url_;

  // Because we didn't process any of the requests to load in Update we're
  // actually at initial_instant_url_. We need to reset url_ so that callers see
  // the correct state.
  url_ = initial_instant_url_;

  frame_load_observer_.reset(NULL);

  delegate_->InstantLoaderDoesntSupportInstant(this, needs_reload, url_to_load);
}

void InstantLoader::ProcessBoundsChange() {
  if (last_omnibox_bounds_ == omnibox_bounds_)
    return;

  last_omnibox_bounds_ = omnibox_bounds_;
  if (preview_contents_.get() && is_showing_instant() &&
      !is_waiting_for_load()) {
    SendOmniboxBoundsScript(preview_contents_.get(),
                            GetOmniboxBoundsInTermsOfPreview());
  }
}
