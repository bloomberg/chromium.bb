// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/match_preview.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/browser/history/history_marshaling.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/match_preview_delegate.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/renderer_preferences.h"
#include "gfx/codec/png_codec.h"
#include "ipc/ipc_message.h"

namespace {

const char kUserInputScript[] =
    "if (window.chrome.userInput) window.chrome.userInput(\"$1\");";
const char kUserDoneScript[] =
    "if (window.chrome.userWantsQuery) window.chrome.userWantsQuery(\"$1\");";
const char kSetOmniboxBoundsScript[] =
    "if (window.chrome.setOmniboxDimensions) "
    "window.chrome.setOmniboxDimensions($1, $2, $3, $4);";

// Sends the user input script to |tab_contents|. |text| is the text the user
// input into the omnibox.
void SendUserInputScript(TabContents* tab_contents,
                         const string16& text,
                         bool done) {
  string16 escaped_text(text);
  ReplaceSubstringsAfterOffset(&escaped_text, 0L, ASCIIToUTF16("\""),
                               ASCIIToUTF16("\\\""));
  string16 script = ReplaceStringPlaceholders(
      ASCIIToUTF16(done ? kUserDoneScript : kUserInputScript),
      escaped_text,
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
class MatchPreview::FrameLoadObserver : public NotificationObserver {
 public:
  FrameLoadObserver(MatchPreview* match_preview, const string16& text)
      : match_preview_(match_preview),
        tab_contents_(match_preview->preview_contents()),
        unique_id_(tab_contents_->controller().pending_entry()->unique_id()),
        text_(text),
        send_done_(false) {
    registrar_.Add(this, NotificationType::LOAD_COMPLETED_MAIN_FRAME,
                   Source<TabContents>(tab_contents_));
    registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(tab_contents_));
  }

  // Sets the text to send to the page.
  void set_text(const string16& text) { text_ = text; }

  // Invoked when the MatchPreview releases ownership of the TabContents and
  // the page hasn't finished loading.
  void DetachFromPreview() {
    match_preview_ = NULL;
    send_done_ = true;
  }

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

        if (match_preview_) {
          gfx::Rect bounds = match_preview_->GetOmniboxBoundsInTermsOfPreview();
          if (!bounds.IsEmpty())
            SendOmniboxBoundsScript(tab_contents_, bounds);
        }

        SendUserInputScript(tab_contents_, text_, send_done_);

        if (match_preview_)
          match_preview_->PageFinishedLoading();

        delete this;
        return;
      }

      case NotificationType::TAB_CONTENTS_DESTROYED:
        delete this;
        return;

      default:
        NOTREACHED();
        break;
    }
  }

 private:
  // MatchPreview that created us.
  MatchPreview* match_preview_;

  // The TabContents we're listening for changes on.
  TabContents* tab_contents_;

  // unique_id of the NavigationEntry we're waiting on.
  const int unique_id_;

  // Text to send down to the page.
  string16 text_;

  // Passed to SendScript.
  bool send_done_;

  // Registers and unregisters us for notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(FrameLoadObserver);
};

// PaintObserver implementation. When the RenderWidgetHost paints itself this
// notifies MatchPreview, which makes the TabContents active.
class MatchPreview::PaintObserverImpl : public RenderWidgetHost::PaintObserver {
 public:
  explicit PaintObserverImpl(MatchPreview* preview)
      : match_preview_(preview) {
  }

  virtual void RenderWidgetHostWillPaint(RenderWidgetHost* rwh) {
  }

  virtual void RenderWidgetHostDidPaint(RenderWidgetHost* rwh) {
    match_preview_->PreviewDidPaint();
    rwh->set_paint_observer(NULL);
    // WARNING: we've been deleted.
  }

 private:
  MatchPreview* match_preview_;

  DISALLOW_COPY_AND_ASSIGN(PaintObserverImpl);
};

class MatchPreview::TabContentsDelegateImpl : public TabContentsDelegate {
 public:
  explicit TabContentsDelegateImpl(MatchPreview* match_preview)
      : match_preview_(match_preview),
        installed_paint_observer_(false),
        waiting_for_new_page_(true) {
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
  }

  // Commits the currently buffered history.
  void CommitHistory() {
    TabContents* tab = match_preview_->preview_contents();
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
          set_paint_observer(new PaintObserverImpl(match_preview_));
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
  virtual TabContents* GetConstrainingContents(TabContents* source) {
    return NULL;
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
  virtual void RenderWidgetShowing() {}
  virtual ExtensionFunctionDispatcher* CreateExtensionFunctionDispatcher(
      RenderViewHost* render_view_host,
      const std::string& extension_id) {
    return NULL;
  }
  virtual bool TakeFocus(bool reverse) { return false; }
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
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}
  virtual void ShowRepostFormWarningDialog(TabContents* tab_contents) {}
  virtual void ShowContentSettingsWindow(ContentSettingsType content_type) {}
  virtual void ShowCollectedCookiesDialog(TabContents* tab_contents) {}
  virtual bool OnGoToEntryOffset(int offset) { return false; }
  virtual bool ShouldAddNavigationsToHistory(
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
    TabContents* source = match_preview_->preview_contents();
    // TODO: only allow for default search provider.
    if (source->controller().GetActiveEntry() &&
        page_id == source->controller().GetActiveEntry()->page_id()) {
      match_preview_->SetCompleteSuggestedText(UTF8ToUTF16(result));
    }
  }

 private:
  typedef std::vector<scoped_refptr<history::HistoryAddPageArgs> >
      AddPageVector;

  MatchPreview* match_preview_;

  // Has the paint observer been installed? See comment in
  // NavigationStateChanged for details on this.
  bool installed_paint_observer_;

  // Used to cache data that needs to be added to history. Normally entries are
  // added to history as the user types, but for match preview we only want to
  // add the items to history if the user commits the match preview. So, we
  // cache them here and if committed then add the items to history.
  AddPageVector add_page_vector_;

  // Are we we waiting for a NavigationType of NEW_PAGE? If we're waiting for
  // NEW_PAGE navigation we don't add history items to add_page_vector_.
  bool waiting_for_new_page_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsDelegateImpl);
};

// static
bool MatchPreview::IsEnabled() {
  static bool enabled = false;
  static bool checked = false;
  if (!checked) {
    checked = true;
    enabled = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableMatchPreview);
  }
  return enabled;
}

MatchPreview::MatchPreview(MatchPreviewDelegate* delegate)
    : delegate_(delegate),
      tab_contents_(NULL),
      is_active_(false),
      template_url_id_(0) {
  preview_tab_contents_delegate_.reset(new TabContentsDelegateImpl(this));
}

MatchPreview::~MatchPreview() {
  // Delete the TabContents before the delegate as the TabContents holds a
  // reference to the delegate.
  preview_contents_.reset(NULL);
}

void MatchPreview::Update(TabContents* tab_contents,
                          const AutocompleteMatch& match,
                          const string16& user_text,
                          string16* suggested_text) {
  if (tab_contents != tab_contents_)
    DestroyPreviewContents();

  tab_contents_ = tab_contents;

  if (url_ == match.destination_url)
    return;

  url_ = match.destination_url;

  if (url_.is_empty() || !url_.is_valid()) {
    DestroyPreviewContents();
    return;
  }

  user_text_ = user_text;

  if (preview_contents_.get() == NULL) {
    preview_contents_.reset(
        new TabContents(tab_contents_->profile(), NULL, MSG_ROUTING_NONE,
                        NULL, NULL));
    // Propagate the max page id. That way if we end up merging the two
    // NavigationControllers (which happens if we commit) none of the page ids
    // will overlap.
    int32 max_page_id = tab_contents_->GetMaxPageID();
    if (max_page_id != -1)
      preview_contents_->controller().set_max_restored_page_id(max_page_id + 1);

    preview_contents_->set_delegate(preview_tab_contents_delegate_.get());

    gfx::Rect tab_bounds;
    tab_contents_->view()->GetContainerBounds(&tab_bounds);
    preview_contents_->view()->SizeContents(tab_bounds.size());

    preview_contents_->ShowContents();
  }
  preview_tab_contents_delegate_->PrepareForNewLoad();

  const TemplateURL* template_url = match.template_url;
  if (match.type == AutocompleteMatch::SEARCH_WHAT_YOU_TYPED ||
      match.type == AutocompleteMatch::SEARCH_HISTORY ||
      match.type == AutocompleteMatch::SEARCH_SUGGEST) {
    TemplateURLModel* model = tab_contents->profile()->GetTemplateURLModel();
    template_url = model ? model->GetDefaultSearchProvider() : NULL;
  }
  TemplateURLID template_url_id = template_url ? template_url->id() : 0;

  if (template_url && template_url->supports_instant() &&
      TemplateURL::SupportsReplacement(template_url)) {
    if (template_url_id == template_url_id_) {
      if (is_waiting_for_load()) {
        // The page hasn't loaded yet. We'll send the script down when it does.
        frame_load_observer_->set_text(user_text_);
        return;
      }
      SendUserInputScript(preview_contents_.get(), user_text_, false);
      if (complete_suggested_text_.size() > user_text_.size() &&
          !complete_suggested_text_.compare(0, user_text_.size(), user_text_)) {
        *suggested_text = complete_suggested_text_.substr(user_text_.size());
      }
    } else {
      // TODO: should we use a different url for instant?
      GURL url = GURL(template_url->url()->ReplaceSearchTerms(
          *template_url, std::wstring(),
          TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()));
      // user_text_ is sent once the page finishes loading by FrameLoadObserver.
      preview_contents_->controller().LoadURL(url, GURL(), match.transition);
      frame_load_observer_.reset(new FrameLoadObserver(this, user_text_));
    }
  } else {
    template_url_id_ = 0;
    frame_load_observer_.reset(NULL);
    preview_contents_->controller().LoadURL(url_, GURL(), match.transition);
  }

  template_url_id_ = template_url_id;
}

void MatchPreview::SetOmniboxBounds(const gfx::Rect& bounds) {
  if (omnibox_bounds_ == bounds)
    return;

  omnibox_bounds_ = bounds;
  if (preview_contents_.get() && is_showing_instant() &&
      !is_waiting_for_load()) {
    SendOmniboxBoundsScript(preview_contents_.get(),
                            GetOmniboxBoundsInTermsOfPreview());
  }
}

void MatchPreview::DestroyPreviewContents() {
  if (!preview_contents_.get()) {
    // We're not showing anything, nothing to do.
    return;
  }

  delegate_->HideMatchPreview();
  delete ReleasePreviewContents(false);
}

void MatchPreview::CommitCurrentPreview() {
  DCHECK(preview_contents_.get());
  delegate_->CommitMatchPreview();
}

TabContents* MatchPreview::ReleasePreviewContents(bool commit_history) {
  omnibox_bounds_ = gfx::Rect();
  template_url_id_ = 0;
  url_ = GURL();
  user_text_.clear();
  complete_suggested_text_.clear();
  if (frame_load_observer_.get()) {
    frame_load_observer_->DetachFromPreview();
    // FrameLoadObserver will delete itself either when the TabContents is
    // deleted, or when the page finishes loading.
    FrameLoadObserver* unused ALLOW_UNUSED = frame_load_observer_.release();
  }
  if (preview_contents_.get()) {
    if (commit_history)
      preview_tab_contents_delegate_->CommitHistory();
    // Destroy the paint observer.
    if (preview_contents_->GetRenderWidgetHostView()) {
      // RenderWidgetHostView may be null during shutdown.
      preview_contents_->GetRenderWidgetHostView()->GetRenderWidgetHost()->
          set_paint_observer(NULL);
    }
    preview_contents_->set_delegate(NULL);
    preview_tab_contents_delegate_->Reset();
    is_active_ = false;
  }
  return preview_contents_.release();
}

void MatchPreview::SetCompleteSuggestedText(
    const string16& complete_suggested_text) {
  if (complete_suggested_text == complete_suggested_text_)
    return;

  if (user_text_.compare(0, user_text_.size(), complete_suggested_text,
                         0, user_text_.size())) {
    // The user text no longer contains the suggested text, ignore it.
    complete_suggested_text_.clear();
    delegate_->SetSuggestedText(string16());
    return;
  }

  complete_suggested_text_ = complete_suggested_text;
  delegate_->SetSuggestedText(
      complete_suggested_text_.substr(user_text_.size()));
}

void MatchPreview::PreviewDidPaint() {
  DCHECK(!is_active_);
  is_active_ = true;
  delegate_->ShowMatchPreview();
}

void MatchPreview::PageFinishedLoading() {
  // FrameLoadObserver deletes itself after this call.
  FrameLoadObserver* unused ALLOW_UNUSED = frame_load_observer_.release();
}

gfx::Rect MatchPreview::GetOmniboxBoundsInTermsOfPreview() {
  if (omnibox_bounds_.IsEmpty())
    return omnibox_bounds_;

  gfx::Rect preview_bounds(delegate_->GetMatchPreviewBounds());
  return gfx::Rect(omnibox_bounds_.x() - preview_bounds.x(),
                   omnibox_bounds_.y() - preview_bounds.y(),
                   omnibox_bounds_.width(),
                   omnibox_bounds_.height());
}
