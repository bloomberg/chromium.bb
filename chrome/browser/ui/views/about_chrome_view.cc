// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/about_chrome_view.h"

#if defined(OS_WIN)
#include <commdlg.h>
#endif  // defined(OS_WIN)

#include <algorithm>
#include <vector>

#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "content/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/layout/layout_constants.h"
#include "views/controls/button/text_button.h"
#include "views/controls/link.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/throbber.h"
#include "views/view_text_utils.h"
#include "views/widget/widget.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/restart_message_box.h"
#include "chrome/installer/util/install_util.h"
#endif  // defined(OS_WIN)

// The amount of vertical space separating the error label at the bottom from
// the rest of the text.
static const int kErrorLabelVerticalSpacing = 15;  // Pixels.

namespace {
// These are used as placeholder text around the links in the text in the about
// dialog.
const string16 kBeginLink(ASCIIToUTF16("BEGIN_LINK"));
const string16 kEndLink(ASCIIToUTF16("END_LINK"));
const string16 kBeginLinkChr(ASCIIToUTF16("BEGIN_LINK_CHR"));
const string16 kBeginLinkOss(ASCIIToUTF16("BEGIN_LINK_OSS"));
const string16 kEndLinkChr(ASCIIToUTF16("END_LINK_CHR"));
const string16 kEndLinkOss(ASCIIToUTF16("END_LINK_OSS"));

// Returns a substring from |text| between start and end.
string16 StringSubRange(const string16& text, size_t start, size_t end) {
  DCHECK(end > start);
  return text.substr(start, end - start);
}

}  // namespace

namespace browser {

// Declared in browser_dialogs.h so that others don't
// need to depend on our .h.
views::Widget* ShowAboutChromeView(gfx::NativeWindow parent, Profile* profile) {
  views::Widget* about_chrome_window =
      browser::CreateViewsWindow(parent, new AboutChromeView(profile));
      about_chrome_window->Show();
  return about_chrome_window;
}

}  // namespace browser

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, public:

AboutChromeView::AboutChromeView(Profile* profile)
    : profile_(profile),
      about_dlg_background_logo_(NULL),
      about_title_label_(NULL),
      version_label_(NULL),
      copyright_label_(NULL),
      main_text_label_(NULL),
      main_text_label_height_(0),
      chromium_url_(NULL),
      open_source_url_(NULL),
      terms_of_service_url_(NULL),
      error_label_(NULL),
      restart_button_visible_(false),
      chromium_url_appears_first_(true),
      text_direction_is_rtl_(false) {
  DCHECK(profile);

  Init();

#if defined(OS_WIN) && !defined(USE_AURA)
  google_updater_ = new GoogleUpdate();
  google_updater_->set_status_listener(this);
#endif
}

AboutChromeView::~AboutChromeView() {
#if defined(OS_WIN) && !defined(USE_AURA)
  // The Google Updater will hold a pointer to us until it reports status, so we
  // need to let it know that we will no longer be listening.
  if (google_updater_)
    google_updater_->set_status_listener(NULL);
#endif
}

void AboutChromeView::Init() {
  text_direction_is_rtl_ = base::i18n::IsRTL();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  // Views we will add to the *parent* of this dialog, since it will display
  // next to the buttons which we don't draw ourselves.
  throbber_.reset(new views::Throbber(50, true));
  throbber_->set_parent_owned(false);
  throbber_->SetVisible(false);

  SkBitmap* success_image = rb.GetBitmapNamed(IDR_UPDATE_UPTODATE);
  success_indicator_.SetImage(*success_image);
  success_indicator_.set_parent_owned(false);

  SkBitmap* update_available_image = rb.GetBitmapNamed(IDR_UPDATE_AVAILABLE);
  update_available_indicator_.SetImage(*update_available_image);
  update_available_indicator_.set_parent_owned(false);

  SkBitmap* timeout_image = rb.GetBitmapNamed(IDR_UPDATE_FAIL);
  timeout_indicator_.SetImage(*timeout_image);
  timeout_indicator_.set_parent_owned(false);

  update_label_.SetVisible(false);
  update_label_.set_parent_owned(false);

  // Regular view controls we draw by ourself. First, we add the background
  // image for the dialog. We have two different background bitmaps, one for
  // LTR UIs and one for RTL UIs. We load the correct bitmap based on the UI
  // layout of the view.
  about_dlg_background_logo_ = new views::ImageView();
  SkBitmap* about_background_logo = rb.GetBitmapNamed(base::i18n::IsRTL() ?
      IDR_ABOUT_BACKGROUND_RTL : IDR_ABOUT_BACKGROUND);

  about_dlg_background_logo_->SetImage(*about_background_logo);
  AddChildView(about_dlg_background_logo_);

  // Add the dialog labels.
  about_title_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  about_title_label_->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(18));
  about_title_label_->SetBackgroundColor(SK_ColorWHITE);
  about_title_label_->SetEnabledColor(SK_ColorBLACK);
  AddChildView(about_title_label_);

  // This is a text field so people can copy the version number from the dialog.
  version_label_ = new views::Textfield();
  chrome::VersionInfo version_info;
  {
    // TODO(finnur): Need to evaluate whether we should be doing IO here.
    //               See issue: http://crbug.com/101699.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    version_label_->SetText(UTF8ToUTF16(version_info.CreateVersionString()));
  }
  version_label_->SetReadOnly(true);
  version_label_->RemoveBorder();
  version_label_->SetTextColor(SK_ColorBLACK);
  version_label_->SetBackgroundColor(SK_ColorWHITE);
  version_label_->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont));
  AddChildView(version_label_);

  // The copyright URL portion of the main label.
  copyright_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COPYRIGHT));
  copyright_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(copyright_label_);

  main_text_label_ = new views::Label(string16());

  // Figure out what to write in the main label of the About box.
  string16 text = l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_LICENSE);

  chromium_url_appears_first_ =
      text.find(kBeginLinkChr) < text.find(kBeginLinkOss);

  size_t link1 = text.find(kBeginLink);
  DCHECK(link1 != string16::npos);
  size_t link1_end = text.find(kEndLink, link1);
  DCHECK(link1_end != string16::npos);
  size_t link2 = text.find(kBeginLink, link1_end);
  DCHECK(link2 != string16::npos);
  size_t link2_end = text.find(kEndLink, link2);
  DCHECK(link1_end != string16::npos);

  main_label_chunk1_ = text.substr(0, link1);
  main_label_chunk2_ = StringSubRange(text, link1_end + kEndLinkOss.size(),
                                      link2);
  main_label_chunk3_ = text.substr(link2_end + kEndLinkOss.size());

  // The Chromium link within the main text of the dialog.
  chromium_url_ = new views::Link(
      StringSubRange(text, text.find(kBeginLinkChr) + kBeginLinkChr.size(),
                     text.find(kEndLinkChr)));
  AddChildView(chromium_url_);
  chromium_url_->set_listener(this);

  // The Open Source link within the main text of the dialog.
  open_source_url_ = new views::Link(
      StringSubRange(text, text.find(kBeginLinkOss) + kBeginLinkOss.size(),
                     text.find(kEndLinkOss)));
  AddChildView(open_source_url_);
  open_source_url_->set_listener(this);

#if defined(OS_WIN)
  SkColor background_color = color_utils::GetSysSkColor(COLOR_3DFACE);
  copyright_label_->SetBackgroundColor(background_color);
  main_text_label_->SetBackgroundColor(background_color);
  chromium_url_->SetBackgroundColor(background_color);
  open_source_url_->SetBackgroundColor(background_color);
#endif

  // Add together all the strings in the dialog for the purpose of calculating
  // the height of the dialog. The space for the Terms of Service string is not
  // included (it is added later, if needed).
  string16 full_text = main_label_chunk1_ + chromium_url_->GetText() +
                       main_label_chunk2_ + open_source_url_->GetText() +
                       main_label_chunk3_;

  dialog_dimensions_ = views::Widget::GetLocalizedContentsSize(
      IDS_ABOUT_DIALOG_WIDTH_CHARS,
      IDS_ABOUT_DIALOG_MINIMUM_HEIGHT_LINES);

  // Create a label and add the full text so we can query it for the height.
  views::Label dummy_text(full_text);
  dummy_text.SetMultiLine(true);
  gfx::Font font =
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);

  // Add up the height of the various elements on the page.
  int height = about_background_logo->height() +
      views::kRelatedControlVerticalSpacing +
      // Copyright line.
      font.GetHeight() +
      // Main label.
      dummy_text.GetHeightForWidth(
          dialog_dimensions_.width() - (2 * views::kPanelHorizMargin)) +
          views::kRelatedControlVerticalSpacing;

#if defined(GOOGLE_CHROME_BUILD)
  std::vector<size_t> url_offsets;
  text = l10n_util::GetStringFUTF16(IDS_ABOUT_TERMS_OF_SERVICE,
                                    string16(),
                                    string16(),
                                    &url_offsets);

  main_label_chunk4_ = text.substr(0, url_offsets[0]);
  main_label_chunk5_ = text.substr(url_offsets[0]);

  // The Terms of Service URL at the bottom.
  terms_of_service_url_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_TERMS_OF_SERVICE));
  AddChildView(terms_of_service_url_);
  terms_of_service_url_->set_listener(this);

  error_label_ = new views::Label();
  error_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  error_label_->SetMultiLine(true);
  AddChildView(error_label_);

  // Add the Terms of Service line and some whitespace.
  height += font.GetHeight() + views::kRelatedControlVerticalSpacing;
#endif

  // Use whichever is greater (the calculated height or the specified minimum
  // height).
  dialog_dimensions_.set_height(std::max(height, dialog_dimensions_.height()));
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, views::View implementation:

gfx::Size AboutChromeView::GetPreferredSize() {
  return dialog_dimensions_;
}

void AboutChromeView::Layout() {
  gfx::Size panel_size = GetPreferredSize();

  // Background image for the dialog.
  gfx::Size sz = about_dlg_background_logo_->GetPreferredSize();
  // Used to position main text below.
  int background_image_height = sz.height();
  about_dlg_background_logo_->SetBounds(panel_size.width() - sz.width(), 0,
                                        sz.width(), sz.height());

  // First label goes to the top left corner.
  sz = about_title_label_->GetPreferredSize();
  about_title_label_->SetBounds(
      views::kPanelHorizMargin, views::kPanelVertMargin,
      sz.width(), sz.height());

  // Then we have the version number right below it.
  sz = version_label_->GetPreferredSize();
  version_label_->SetBounds(views::kPanelHorizMargin,
                            about_title_label_->y() +
                                about_title_label_->height() +
                                views::kRelatedControlVerticalSpacing,
                            panel_size.width() -
                                about_dlg_background_logo_->width(),
                            sz.height());

  // For the width of the main text label we want to use up the whole panel
  // width and remaining height, minus a little margin on each side.
  int y_pos = background_image_height + views::kRelatedControlVerticalSpacing;
  sz.set_width(panel_size.width() - 2 * views::kPanelHorizMargin);

  // Draw the text right below the background image.
  copyright_label_->SetBounds(views::kPanelHorizMargin,
                              y_pos,
                              sz.width(),
                              sz.height());

  // Then the main_text_label.
  main_text_label_->SetBounds(views::kPanelHorizMargin,
                              copyright_label_->y() +
                                  copyright_label_->height(),
                              sz.width(),
                              main_text_label_height_);

  // And the error label at the bottom of the main content. This does not fit on
  // screen until EnlargeWindowSizeIfNeeded has been called (which happens when
  // an error is returned from Google Update).
  if (error_label_) {
    sz.set_height(error_label_->GetHeightForWidth(sz.width()));
    error_label_->SetBounds(main_text_label_->bounds().x(),
                            main_text_label_->bounds().y() +
                            main_text_label_->height() +
                            kErrorLabelVerticalSpacing,
                            sz.width(), sz.height());
  }

  // Get the y-coordinate of our parent so we can position the text left of the
  // buttons at the bottom.
  gfx::Rect parent_bounds = parent()->GetContentsBounds();

  sz = throbber_->GetPreferredSize();
  int throbber_topleft_x = views::kPanelHorizMargin;
  int throbber_topleft_y =
      parent_bounds.bottom() - sz.height() - views::kButtonVEdgeMargin - 3;
  throbber_->SetBounds(throbber_topleft_x, throbber_topleft_y,
                       sz.width(), sz.height());

  // This image is hidden (see ViewHierarchyChanged) and displayed on demand.
  sz = success_indicator_.GetPreferredSize();
  success_indicator_.SetBounds(throbber_topleft_x, throbber_topleft_y,
                               sz.width(), sz.height());

  // This image is hidden (see ViewHierarchyChanged) and displayed on demand.
  sz = update_available_indicator_.GetPreferredSize();
  update_available_indicator_.SetBounds(throbber_topleft_x, throbber_topleft_y,
                                        sz.width(), sz.height());

  // This image is hidden (see ViewHierarchyChanged) and displayed on demand.
  sz = timeout_indicator_.GetPreferredSize();
  timeout_indicator_.SetBounds(throbber_topleft_x, throbber_topleft_y,
                               sz.width(), sz.height());

  // The update label should be at the bottom of the screen, to the right of the
  // throbber. It vertically lines up with the ok button, so we make sure it
  // doesn't extend into the ok button.
  sz = update_label_.GetPreferredSize();
  int update_label_x = throbber_->x() + throbber_->width() +
                       views::kRelatedControlHorizontalSpacing;
  views::DialogClientView* dialog_client_view = GetDialogClientView();
  int max_x = parent_bounds.width();
  if (dialog_client_view->ok_button() &&
      dialog_client_view->ok_button()->IsVisible()) {
    max_x = std::min(dialog_client_view->ok_button()->x(), max_x);
  }
  if (dialog_client_view->cancel_button() &&
      dialog_client_view->cancel_button()->IsVisible()) {
    max_x = std::min(dialog_client_view->cancel_button()->x(), max_x);
  }
  update_label_.SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  update_label_.SetBounds(update_label_x,
                          throbber_topleft_y + 1,
                          std::min(max_x - update_label_x, sz.width()),
                          sz.height());
}

void AboutChromeView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  // Draw the background image color (and the separator) across the dialog.
  // This will become the background for the logo image at the top of the
  // dialog.
  SkBitmap* background = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_ABOUT_BACKGROUND_COLOR);
  canvas->TileImageInt(*background, 0, 0, dialog_dimensions_.width(),
                       background->height());

  gfx::Font font =
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);

  const gfx::Rect label_bounds = main_text_label_->bounds();

  views::Link* link1 =
      chromium_url_appears_first_ ? chromium_url_ : open_source_url_;
  views::Link* link2 =
      chromium_url_appears_first_ ? open_source_url_ : chromium_url_;
  gfx::Rect* rect1 = chromium_url_appears_first_ ?
      &chromium_url_rect_ : &open_source_url_rect_;
  gfx::Rect* rect2 = chromium_url_appears_first_ ?
      &open_source_url_rect_ : &chromium_url_rect_;

  // This struct keeps track of where to write the next word (which x,y
  // pixel coordinate). This struct is updated after drawing text and checking
  // if we need to wrap.
  gfx::Size position;
  // Draw the first text chunk and position the Chromium url.
  view_text_utils::DrawTextAndPositionUrl(canvas, main_text_label_,
      main_label_chunk1_, link1, rect1, &position,
      text_direction_is_rtl_, label_bounds, font);
  // Draw the second text chunk and position the Open Source url.
  view_text_utils::DrawTextAndPositionUrl(canvas, main_text_label_,
      main_label_chunk2_, link2, rect2, &position,
      text_direction_is_rtl_, label_bounds, font);
  // Draw the third text chunk (which has no URL associated with it).
  view_text_utils::DrawTextAndPositionUrl(canvas, main_text_label_,
      main_label_chunk3_, NULL, NULL, &position,
      text_direction_is_rtl_, label_bounds, font);

#if defined(GOOGLE_CHROME_BUILD)
  // Insert a line break and some whitespace.
  position.set_width(0);
  position.Enlarge(0, font.GetHeight() + views::kRelatedControlVerticalSpacing);

  // And now the Terms of Service and position the TOS url.
  view_text_utils::DrawTextAndPositionUrl(canvas, main_text_label_,
      main_label_chunk4_, terms_of_service_url_,
      &terms_of_service_url_rect_, &position, text_direction_is_rtl_,
      label_bounds, font);
  // The last text chunk doesn't have a URL associated with it.
  view_text_utils::DrawTextAndPositionUrl(canvas, main_text_label_,
       main_label_chunk5_, NULL, NULL, &position,
       text_direction_is_rtl_, label_bounds, font);

  // Position the TOS URL within the main label.
  terms_of_service_url_->SetBounds(terms_of_service_url_rect_.x(),
                                   terms_of_service_url_rect_.y(),
                                   terms_of_service_url_rect_.width(),
                                   terms_of_service_url_rect_.height());
#endif

  // Position the URLs within the main label. First position the Chromium URL
  // within the main label.
  chromium_url_->SetBounds(chromium_url_rect_.x(),
                           chromium_url_rect_.y(),
                           chromium_url_rect_.width(),
                           chromium_url_rect_.height());
  // Then position the Open Source URL within the main label.
  open_source_url_->SetBounds(open_source_url_rect_.x(),
                              open_source_url_rect_.y(),
                              open_source_url_rect_.width(),
                              open_source_url_rect_.height());

  // Save the height so we can set the bounds correctly.
  main_text_label_height_ = position.height() + font.GetHeight();
}

void AboutChromeView::ViewHierarchyChanged(bool is_add,
                                           views::View* parent,
                                           views::View* child) {
  // Since we want some of the controls to show up in the same visual row
  // as the buttons, which are provided by the framework, we must add the
  // buttons to the non-client view, which is the parent of this view.
  // Similarly, when we're removed from the view hierarchy, we must take care
  // to remove these items as well.
  if (child == this) {
    if (is_add) {
      parent->AddChildView(&update_label_);
      parent->AddChildView(throbber_.get());
      parent->AddChildView(&success_indicator_);
      success_indicator_.SetVisible(false);
      parent->AddChildView(&update_available_indicator_);
      update_available_indicator_.SetVisible(false);
      parent->AddChildView(&timeout_indicator_);
      timeout_indicator_.SetVisible(false);

#if defined(OS_WIN) && !defined(USE_AURA)
      // On-demand updates for Chrome don't work in Vista RTM when UAC is turned
      // off. So, in this case we just want the About box to not mention
      // on-demand updates. Silent updates (in the background) should still
      // work as before - enabling UAC or installing the latest service pack
      // for Vista is another option.
      if (!(base::win::GetVersion() == base::win::VERSION_VISTA &&
            (base::win::OSInfo::GetInstance()->service_pack().major == 0) &&
            !base::win::UserAccountControlIsEnabled())) {
        UpdateStatus(UPGRADE_CHECK_STARTED, GOOGLE_UPDATE_NO_ERROR, string16());
        // CheckForUpdate(false, ...) means don't upgrade yet.
        google_updater_->CheckForUpdate(false, GetWidget());
      }
#endif
    } else {
      parent->RemoveChildView(&update_label_);
      parent->RemoveChildView(throbber_.get());
      parent->RemoveChildView(&success_indicator_);
      parent->RemoveChildView(&update_available_indicator_);
      parent->RemoveChildView(&timeout_indicator_);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, views::DialogDelegate implementation:

string16 AboutChromeView::GetDialogButtonLabel(ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    return l10n_util::GetStringUTF16(IDS_RELAUNCH_AND_UPDATE);
  } else if (button == ui::DIALOG_BUTTON_CANCEL) {
    if (restart_button_visible_)
      return l10n_util::GetStringUTF16(IDS_NOT_NOW);
    // The OK button (which is the default button) has been re-purposed to be
    // 'Restart Now' so we want the Cancel button should have the label
    // OK but act like a Cancel button in all other ways.
    return l10n_util::GetStringUTF16(IDS_OK);
  }

  NOTREACHED();
  return string16();
}

string16 AboutChromeView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_ABOUT_CHROME_TITLE);
}

bool AboutChromeView::IsDialogButtonEnabled(ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK && !restart_button_visible_)
    return false;

  return true;
}

bool AboutChromeView::IsDialogButtonVisible(ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK && !restart_button_visible_)
    return false;

  return true;
}

// (on ChromeOS) the default focus is ending up in the version field when
// the update button is hidden. This forces the focus to always be on the
// OK button (which is the dialog cancel button, see GetDialogButtonLabel
// above).
int AboutChromeView::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

bool AboutChromeView::CanResize() const {
  return false;
}

bool AboutChromeView::CanMaximize() const {
  return false;
}

bool AboutChromeView::IsModal() const {
  return true;
}

bool AboutChromeView::Accept() {
  BrowserList::AttemptRestart();
  return true;
}

views::View* AboutChromeView::GetContentsView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, views::LinkListener implementation:
void AboutChromeView::LinkClicked(views::Link* source, int event_flags) {
  GURL url;
  if (source == terms_of_service_url_) {
    url = GURL(chrome::kChromeUITermsURL);
  } else if (source == chromium_url_) {
    url = google_util::AppendGoogleLocaleParam(
      GURL(chrome::kChromiumProjectURL));
  } else if (source == open_source_url_) {
    url = GURL(chrome::kChromeUICreditsURL);
  } else {
    NOTREACHED() << "Unknown link source";
  }

  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  browser->OpenURL(url, GURL(), NEW_WINDOW, content::PAGE_TRANSITION_LINK);
}

#if defined(OS_WIN) && !defined(USE_AURA)
////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, GoogleUpdateStatusListener implementation:

void AboutChromeView::OnReportResults(GoogleUpdateUpgradeResult result,
                                      GoogleUpdateErrorCode error_code,
                                      const string16& error_message,
                                      const string16& version) {
  // Drop the last reference to the object so that it gets cleaned up here.
  google_updater_ = NULL;

  // Make a note of which version Google Update is reporting is the latest
  // version.
  new_version_available_ = version;
  UpdateStatus(result, error_code, error_message);
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, private:

void AboutChromeView::UpdateStatus(GoogleUpdateUpgradeResult result,
                                   GoogleUpdateErrorCode error_code,
                                   const string16& error_message) {
#if !defined(GOOGLE_CHROME_BUILD)
  // For Chromium builds it would show an error message.
  // But it looks weird because in fact there is no error,
  // just the update server is not available for non-official builds.
  return;
#endif
  bool show_success_indicator = false;
  bool show_update_available_indicator = false;
  bool show_timeout_indicator = false;
  bool show_throbber = false;
  bool show_update_label = true;  // Always visible, except at start.

  switch (result) {
    case UPGRADE_STARTED:
      UserMetrics::RecordAction(UserMetricsAction("Upgrade_Started"));
      show_throbber = true;
      update_label_.SetText(
          UTF16ToWide(l10n_util::GetStringUTF16(IDS_UPGRADE_STARTED)));
      break;
    case UPGRADE_CHECK_STARTED:
      UserMetrics::RecordAction(UserMetricsAction("UpgradeCheck_Started"));
      show_throbber = true;
      update_label_.SetText(
          UTF16ToWide(l10n_util::GetStringUTF16(IDS_UPGRADE_CHECK_STARTED)));
      break;
    case UPGRADE_IS_AVAILABLE:
      UserMetrics::RecordAction(
          UserMetricsAction("UpgradeCheck_UpgradeIsAvailable"));
      DCHECK(!google_updater_);  // Should have been nulled out already.
      google_updater_ = new GoogleUpdate();
      google_updater_->set_status_listener(this);
      UpdateStatus(UPGRADE_STARTED, GOOGLE_UPDATE_NO_ERROR, string16());
      // CheckForUpdate(true,...) means perform upgrade if new version found.
      google_updater_->CheckForUpdate(true, GetWidget());
      // TODO(seanparent): Need to see if this code needs to change to
      // force a machine restart.
      return;
    case UPGRADE_ALREADY_UP_TO_DATE: {
      // The extra version check is necessary on Windows because the application
      // may be already up to date on disk though the running app is still
      // out of date. Chrome OS doesn't quite have this issue since the
      // OS/App are updated together. If a newer version of the OS has been
      // staged then UPGRADE_SUCESSFUL will be returned.
      // Google Update reported that Chrome is up-to-date. Now make sure that we
      // are running the latest version and if not, notify the user by falling
      // into the next case of UPGRADE_SUCCESSFUL.
      BrowserDistribution* dist = BrowserDistribution::GetDistribution();
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      chrome::VersionInfo version_info;
      scoped_ptr<Version> installed_version(
          InstallUtil::GetChromeVersion(dist, false));
      if (!installed_version.get()) {
        // User-level Chrome is not installed, check system-level.
        installed_version.reset(InstallUtil::GetChromeVersion(dist, true));
      }
      scoped_ptr<Version> running_version(
          Version::GetVersionFromString(version_info.Version()));
      if (!installed_version.get() ||
          (installed_version->CompareTo(*running_version) <= 0)) {
        UserMetrics::RecordAction(
            UserMetricsAction("UpgradeCheck_AlreadyUpToDate"));
        string16 update_label_text = l10n_util::GetStringFUTF16(
            IDS_UPGRADE_ALREADY_UP_TO_DATE,
            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
            ASCIIToUTF16(version_info.Version()));
        if (base::i18n::IsRTL()) {
          update_label_text.push_back(
              static_cast<wchar_t>(base::i18n::kLeftToRightMark));
        }
        update_label_.SetText(update_label_text);
        show_success_indicator = true;
        break;
      }
      // No break here as we want to notify user about upgrade if there is one.
    }
    case UPGRADE_SUCCESSFUL: {
      if (result == UPGRADE_ALREADY_UP_TO_DATE)
        UserMetrics::RecordAction(
            UserMetricsAction("UpgradeCheck_AlreadyUpgraded"));
      else
        UserMetrics::RecordAction(UserMetricsAction("UpgradeCheck_Upgraded"));
      restart_button_visible_ = true;
      const string16& update_string =
          UTF16ToWide(l10n_util::GetStringFUTF16(
              IDS_UPGRADE_SUCCESSFUL_RELAUNCH,
              l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
      update_label_.SetText(update_string);
      show_success_indicator = true;
      break;
    }
    case UPGRADE_ERROR: {
      UserMetrics::RecordAction(UserMetricsAction("UpgradeCheck_Error"));
      if (!error_message.empty() && error_label_) {
        error_label_->SetText(
            l10n_util::GetStringFUTF16(IDS_ABOUT_BOX_ERROR_DURING_UPDATE_CHECK,
                error_message));
        int added_height = EnlargeWindowSizeIfNeeded();
        dialog_dimensions_.set_height(dialog_dimensions_.height() +
                                      added_height);
      }
      restart_button_visible_ = false;
      if (error_code != GOOGLE_UPDATE_DISABLED_BY_POLICY) {
        update_label_.SetText(
            l10n_util::GetStringFUTF16Int(IDS_UPGRADE_ERROR, error_code));
      } else {
        update_label_.SetText(
            l10n_util::GetStringUTF16(IDS_UPGRADE_DISABLED_BY_POLICY));
      }
      show_timeout_indicator = true;
      break;
    }
    default:
      NOTREACHED();
  }

  success_indicator_.SetVisible(show_success_indicator);
  update_available_indicator_.SetVisible(show_update_available_indicator);
  timeout_indicator_.SetVisible(show_timeout_indicator);
  update_label_.SetVisible(show_update_label);
  throbber_->SetVisible(show_throbber);
  if (show_throbber)
    throbber_->Start();
  else
    throbber_->Stop();

  // We have updated controls on the parent, so we need to update its layout.
  parent()->Layout();

  // Check button may have appeared/disappeared. We cannot call this during
  // ViewHierarchyChanged because the view hasn't been added to a Widget yet.
  if (GetWidget())
    GetDialogClientView()->UpdateDialogButtons();
}

int AboutChromeView::EnlargeWindowSizeIfNeeded() {
  if (!error_label_ || error_label_->GetText().empty())
    return 0;

  // This will enlarge the window each time the function is called, which is
  // fine since we only receive status once from Google Update.
  gfx::Rect window_rect = GetWidget()->GetWindowScreenBounds();
  int height = error_label_->GetHeightForWidth(
      dialog_dimensions_.width() - (2 * views::kPanelHorizMargin)) +
          views::kRelatedControlVerticalSpacing;
  window_rect.set_height(window_rect.height() + height);
  GetWidget()->SetBounds(window_rect);

  return height;
}

#endif
