// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/about_chrome_view.h"

#if defined(OS_WIN)
#include <commdlg.h>
#endif  // defined(OS_WIN)

#include <algorithm>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/throbber.h"
#include "views/layout/layout_constants.h"
#include "views/view_text_utils.h"
#include "views/widget/widget.h"
#include "views/window/window.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "chrome/browser/ui/views/restart_message_box.h"
#include "chrome/installer/util/install_util.h"
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#endif  // defined(OS_WIN) || defined(OS_CHROMEOS)

namespace {
// The pixel width of the version text field. Ideally, we'd like to have the
// bounds set to the edge of the icon. However, the icon is not a view but a
// part of the background, so we have to hard code the width to make sure
// the version field doesn't overlap it.
const int kVersionFieldWidth = 195;

// These are used as placeholder text around the links in the text in the about
// dialog.
const wchar_t* kBeginLink = L"BEGIN_LINK";
const wchar_t* kEndLink = L"END_LINK";
const wchar_t* kBeginLinkChr = L"BEGIN_LINK_CHR";
const wchar_t* kBeginLinkOss = L"BEGIN_LINK_OSS";
const wchar_t* kEndLinkChr = L"END_LINK_CHR";
const wchar_t* kEndLinkOss = L"END_LINK_OSS";

// The background bitmap used to draw the background color for the About box
// and the separator line (this is the image we will draw the logo on top of).
static const SkBitmap* kBackgroundBmp = NULL;

// Returns a substring from |text| between start and end.
std::wstring StringSubRange(const std::wstring& text, size_t start,
                            size_t end) {
  DCHECK(end > start);
  return text.substr(start, end - start);
}

}  // namespace

namespace browser {

  // Declared in browser_dialogs.h so that others don't
  // need to depend on our .h.
  views::Window* ShowAboutChromeView(gfx::NativeWindow parent,
                                     Profile* profile) {
      views::Window* about_chrome_window =
        browser::CreateViewsWindow(parent,
        gfx::Rect(),
        new AboutChromeView(profile));
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
#if defined(OS_CHROMEOS)
      os_version_label_(NULL),
#endif
      copyright_label_(NULL),
      main_text_label_(NULL),
      main_text_label_height_(0),
      chromium_url_(NULL),
      open_source_url_(NULL),
      terms_of_service_url_(NULL),
      restart_button_visible_(false),
      chromium_url_appears_first_(true),
      text_direction_is_rtl_(false) {
  DCHECK(profile);
#if defined(OS_CHROMEOS)
  loader_.GetVersion(&consumer_,
                     NewCallback(this, &AboutChromeView::OnOSVersion),
                     chromeos::VersionLoader::VERSION_FULL);
#endif
  Init();

#if defined(OS_WIN) || defined(OS_CHROMEOS)
  google_updater_ = new GoogleUpdate();
  google_updater_->set_status_listener(this);
#endif

  if (kBackgroundBmp == NULL) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    kBackgroundBmp = rb.GetBitmapNamed(IDR_ABOUT_BACKGROUND_COLOR);
  }
}

AboutChromeView::~AboutChromeView() {
#if defined(OS_WIN) || defined(OS_CHROMEOS)
  // The Google Updater will hold a pointer to us until it reports status, so we
  // need to let it know that we will no longer be listening.
  if (google_updater_)
    google_updater_->set_status_listener(NULL);
#endif
}

void AboutChromeView::Init() {
  text_direction_is_rtl_ = base::i18n::IsRTL();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  chrome::VersionInfo version_info;
  if (!version_info.is_valid()) {
    NOTREACHED() << L"Failed to initialize about window";
    return;
  }

  current_version_ = version_info.Version();

  // This code only runs as a result of the user opening the About box so
  // doing registry access to get the version string modifier should be fine.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  std::string version_modifier = platform_util::GetVersionStringModifier();
  if (!version_modifier.empty())
    version_details_ += " " + version_modifier;

#if !defined(GOOGLE_CHROME_BUILD)
  version_details_ += " (";
  version_details_ += version_info.LastChange();
  version_details_ += ")";
#endif

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
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  about_title_label_->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(18));
  about_title_label_->SetColor(SK_ColorBLACK);
  AddChildView(about_title_label_);

  // This is a text field so people can copy the version number from the dialog.
  version_label_ = new views::Textfield();
  version_label_->SetText(ASCIIToUTF16(current_version_ + version_details_));
  version_label_->SetReadOnly(true);
  version_label_->RemoveBorder();
  version_label_->SetTextColor(SK_ColorBLACK);
  version_label_->SetBackgroundColor(SK_ColorWHITE);
  version_label_->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont));
  AddChildView(version_label_);

#if defined(OS_CHROMEOS)
  os_version_label_ = new views::Textfield(views::Textfield::STYLE_MULTILINE);
  os_version_label_->SetReadOnly(true);
  os_version_label_->RemoveBorder();
  os_version_label_->SetTextColor(SK_ColorBLACK);
  os_version_label_->SetBackgroundColor(SK_ColorWHITE);
  os_version_label_->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont));
  AddChildView(os_version_label_);
#endif

  // The copyright URL portion of the main label.
  copyright_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COPYRIGHT)));
  copyright_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(copyright_label_);

  main_text_label_ = new views::Label(L"");

  // Figure out what to write in the main label of the About box.
  std::wstring text =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_LICENSE));

  chromium_url_appears_first_ =
      text.find(kBeginLinkChr) < text.find(kBeginLinkOss);

  size_t link1 = text.find(kBeginLink);
  DCHECK(link1 != std::wstring::npos);
  size_t link1_end = text.find(kEndLink, link1);
  DCHECK(link1_end != std::wstring::npos);
  size_t link2 = text.find(kBeginLink, link1_end);
  DCHECK(link2 != std::wstring::npos);
  size_t link2_end = text.find(kEndLink, link2);
  DCHECK(link1_end != std::wstring::npos);

  main_label_chunk1_ = text.substr(0, link1);
  main_label_chunk2_ = StringSubRange(text, link1_end + wcslen(kEndLinkOss),
                                      link2);
  main_label_chunk3_ = text.substr(link2_end + wcslen(kEndLinkOss));

  // The Chromium link within the main text of the dialog.
  chromium_url_ = new views::Link(
      StringSubRange(text, text.find(kBeginLinkChr) + wcslen(kBeginLinkChr),
                     text.find(kEndLinkChr)));
  AddChildView(chromium_url_);
  chromium_url_->SetController(this);

  // The Open Source link within the main text of the dialog.
  open_source_url_ = new views::Link(
      StringSubRange(text, text.find(kBeginLinkOss) + wcslen(kBeginLinkOss),
                     text.find(kEndLinkOss)));
  AddChildView(open_source_url_);
  open_source_url_->SetController(this);

  // Add together all the strings in the dialog for the purpose of calculating
  // the height of the dialog. The space for the Terms of Service string is not
  // included (it is added later, if needed).
  std::wstring full_text = main_label_chunk1_ + chromium_url_->GetText() +
                           main_label_chunk2_ + open_source_url_->GetText() +
                           main_label_chunk3_;

  dialog_dimensions_ = views::Window::GetLocalizedContentsSize(
      IDS_ABOUT_DIALOG_WIDTH_CHARS,
      IDS_ABOUT_DIALOG_MINIMUM_HEIGHT_LINES);

  // Create a label and add the full text so we can query it for the height.
  views::Label dummy_text(full_text);
  dummy_text.SetMultiLine(true);
  gfx::Font font =
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);

  // Add up the height of the various elements on the page.
  int height = about_background_logo->height() +
               kRelatedControlVerticalSpacing +
               // Copyright line.
               font.GetHeight() +
               // Main label.
               dummy_text.GetHeightForWidth(
                   dialog_dimensions_.width() - (2 * kPanelHorizMargin)) +
               kRelatedControlVerticalSpacing;

#if defined(GOOGLE_CHROME_BUILD)
  std::vector<size_t> url_offsets;
  text = UTF16ToWide(l10n_util::GetStringFUTF16(IDS_ABOUT_TERMS_OF_SERVICE,
                                                string16(),
                                                string16(),
                                                &url_offsets));

  main_label_chunk4_ = text.substr(0, url_offsets[0]);
  main_label_chunk5_ = text.substr(url_offsets[0]);

  // The Terms of Service URL at the bottom.
  terms_of_service_url_ = new views::Link(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_TERMS_OF_SERVICE)));
  AddChildView(terms_of_service_url_);
  terms_of_service_url_->SetController(this);

  // Add the Terms of Service line and some whitespace.
  height += font.GetHeight() + kRelatedControlVerticalSpacing;
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
  about_title_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
                                sz.width(), sz.height());

  // Then we have the version number right below it.
  sz = version_label_->GetPreferredSize();
  version_label_->SetBounds(kPanelHorizMargin,
                            about_title_label_->y() +
                                about_title_label_->height() +
                                kRelatedControlVerticalSpacing,
                            kVersionFieldWidth,
                            sz.height());

#if defined(OS_CHROMEOS)
  // Then we have the version number right below it.
  sz = os_version_label_->GetPreferredSize();
  os_version_label_->SetBounds(
      kPanelHorizMargin,
      version_label_->y() +
          version_label_->height() +
          kRelatedControlVerticalSpacing,
      kVersionFieldWidth,
      sz.height());
#endif

  // For the width of the main text label we want to use up the whole panel
  // width and remaining height, minus a little margin on each side.
  int y_pos = background_image_height + kRelatedControlVerticalSpacing;
  sz.set_width(panel_size.width() - 2 * kPanelHorizMargin);

  // Draw the text right below the background image.
  copyright_label_->SetBounds(kPanelHorizMargin,
                              y_pos,
                              sz.width(),
                              sz.height());

  // Then the main_text_label.
  main_text_label_->SetBounds(kPanelHorizMargin,
                              copyright_label_->y() +
                                  copyright_label_->height(),
                              sz.width(),
                              main_text_label_height_);

  // Get the y-coordinate of our parent so we can position the text left of the
  // buttons at the bottom.
  gfx::Rect parent_bounds = GetParent()->GetLocalBounds();

  sz = throbber_->GetPreferredSize();
  int throbber_topleft_x = kPanelHorizMargin;
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

  // The update label should be at the bottom of the screen, to the right of
  // the throbber. We specify width to the end of the dialog because it contains
  // variable length messages.
  sz = update_label_.GetPreferredSize();
  int update_label_x = throbber_->x() + throbber_->width() +
                       kRelatedControlHorizontalSpacing;
  update_label_.SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  update_label_.SetBounds(update_label_x,
                          throbber_topleft_y + 1,
                          parent_bounds.width() - update_label_x,
                          sz.height());
}


void AboutChromeView::Paint(gfx::Canvas* canvas) {
  views::View::Paint(canvas);

  // Draw the background image color (and the separator) across the dialog.
  // This will become the background for the logo image at the top of the
  // dialog.
  canvas->TileImageInt(*kBackgroundBmp, 0, 0,
                       dialog_dimensions_.width(), kBackgroundBmp->height());

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
      main_label_chunk1_, link1, rect1, &position, text_direction_is_rtl_,
      label_bounds, font);
  // Draw the second text chunk and position the Open Source url.
  view_text_utils::DrawTextAndPositionUrl(canvas, main_text_label_,
      main_label_chunk2_, link2, rect2, &position, text_direction_is_rtl_,
      label_bounds, font);
  // Draw the third text chunk (which has no URL associated with it).
  view_text_utils::DrawTextAndPositionUrl(canvas, main_text_label_,
      main_label_chunk3_, NULL, NULL, &position, text_direction_is_rtl_,
      label_bounds, font);

#if defined(GOOGLE_CHROME_BUILD)
  // Insert a line break and some whitespace.
  position.set_width(0);
  position.Enlarge(0, font.GetHeight() + kRelatedControlVerticalSpacing);

  // And now the Terms of Service and position the TOS url.
  view_text_utils::DrawTextAndPositionUrl(canvas, main_text_label_,
      main_label_chunk4_, terms_of_service_url_, &terms_of_service_url_rect_,
      &position, text_direction_is_rtl_, label_bounds, font);
  // The last text chunk doesn't have a URL associated with it.
  view_text_utils::DrawTextAndPositionUrl(canvas, main_text_label_,
       main_label_chunk5_, NULL, NULL, &position, text_direction_is_rtl_,
       label_bounds, font);

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

#if defined(OS_WIN)
      // On-demand updates for Chrome don't work in Vista RTM when UAC is turned
      // off. So, in this case we just want the About box to not mention
      // on-demand updates. Silent updates (in the background) should still
      // work as before - enabling UAC or installing the latest service pack
      // for Vista is another option.
      int service_pack_major = 0, service_pack_minor = 0;
      base::win::GetServicePackLevel(&service_pack_major, &service_pack_minor);
      if (base::win::UserAccountControlIsEnabled() ||
          base::win::GetVersion() == base::win::VERSION_XP ||
          (base::win::GetVersion() == base::win::VERSION_VISTA &&
           service_pack_major >= 1) ||
          base::win::GetVersion() > base::win::VERSION_VISTA) {
        UpdateStatus(UPGRADE_CHECK_STARTED, GOOGLE_UPDATE_NO_ERROR);
        // CheckForUpdate(false, ...) means don't upgrade yet.
        google_updater_->CheckForUpdate(false, window());
      }
#elif defined(OS_CHROMEOS)
      UpdateStatus(UPGRADE_CHECK_STARTED, GOOGLE_UPDATE_NO_ERROR);
      // CheckForUpdate(false, ...) means don't upgrade yet.
      google_updater_->CheckForUpdate(false, window());
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

std::wstring AboutChromeView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_RESTART_AND_UPDATE));
  } else if (button == MessageBoxFlags::DIALOGBUTTON_CANCEL) {
    if (restart_button_visible_)
      return UTF16ToWide(l10n_util::GetStringUTF16(IDS_NOT_NOW));
    // The OK button (which is the default button) has been re-purposed to be
    // 'Restart Now' so we want the Cancel button should have the label
    // OK but act like a Cancel button in all other ways.
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_OK));
  }

  NOTREACHED();
  return L"";
}

std::wstring AboutChromeView::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_ABOUT_CHROME_TITLE));
}

bool AboutChromeView::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK && !restart_button_visible_)
    return false;

  return true;
}

bool AboutChromeView::IsDialogButtonVisible(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK && !restart_button_visible_)
    return false;

  return true;
}

// (on ChromeOS) the default focus is ending up in the version field when
// the update button is hidden. This forces the focus to always be on the
// OK button (which is the dialog cancel button, see GetDialogButtonLabel
// above).
int AboutChromeView::GetDefaultDialogButton() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

bool AboutChromeView::CanResize() const {
  return false;
}

bool AboutChromeView::CanMaximize() const {
  return false;
}

bool AboutChromeView::IsAlwaysOnTop() const {
  return false;
}

bool AboutChromeView::HasAlwaysOnTopMenu() const {
  return false;
}

bool AboutChromeView::IsModal() const {
  return true;
}

bool AboutChromeView::Accept() {
#if defined(OS_WIN) || defined(OS_CHROMEOS)
  // Set the flag to restore the last session on shutdown.
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);
  BrowserList::CloseAllBrowsersAndExit();
#endif

  return true;
}

views::View* AboutChromeView::GetContentsView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, views::LinkController implementation:

void AboutChromeView::LinkActivated(views::Link* source,
                                    int event_flags) {
  GURL url;
  if (source == terms_of_service_url_) {
    url = GURL(chrome::kAboutTermsURL);
  } else if (source == chromium_url_) {
    url = google_util::AppendGoogleLocaleParam(
      GURL(chrome::kChromiumProjectURL));
  } else if (source == open_source_url_) {
    url = GURL(chrome::kAboutCreditsURL);
  } else {
    NOTREACHED() << "Unknown link source";
  }

  Browser* browser = BrowserList::GetLastActive();
#if defined(OS_CHROMEOS)
  browser->OpenURL(url, GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
#else
  browser->OpenURL(url, GURL(), NEW_WINDOW, PageTransition::LINK);
#endif
}

#if defined(OS_CHROMEOS)
void AboutChromeView::OnOSVersion(
    chromeos::VersionLoader::Handle handle,
    std::string version) {

  // This is a hack to "wrap" the very long Test Build version after
  // the version number, the remaining text won't be visible but can
  // be selected, copied, pasted.
  std::string::size_type pos = version.find(" (Test Build");
  if (pos != std::string::npos)
    version.replace(pos, 1, "\n");

  os_version_label_->SetText(UTF8ToUTF16(version));
}
#endif

#if defined(OS_WIN) || defined(OS_CHROMEOS)
////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, GoogleUpdateStatusListener implementation:

void AboutChromeView::OnReportResults(GoogleUpdateUpgradeResult result,
                                      GoogleUpdateErrorCode error_code,
                                      const std::wstring& version) {
  // Drop the last reference to the object so that it gets cleaned up here.
  google_updater_ = NULL;

  // Make a note of which version Google Update is reporting is the latest
  // version.
  new_version_available_ = version;
  UpdateStatus(result, error_code);
}
////////////////////////////////////////////////////////////////////////////////
// AboutChromeView, private:

void AboutChromeView::UpdateStatus(GoogleUpdateUpgradeResult result,
                                   GoogleUpdateErrorCode error_code) {
#if !defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)
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
      UserMetrics::RecordAction(UserMetricsAction("Upgrade_Started"), profile_);
      show_throbber = true;
      update_label_.SetText(
          UTF16ToWide(l10n_util::GetStringUTF16(IDS_UPGRADE_STARTED)));
      break;
    case UPGRADE_CHECK_STARTED:
      UserMetrics::RecordAction(UserMetricsAction("UpgradeCheck_Started"),
                                profile_);
      show_throbber = true;
      update_label_.SetText(
          UTF16ToWide(l10n_util::GetStringUTF16(IDS_UPGRADE_CHECK_STARTED)));
      break;
    case UPGRADE_IS_AVAILABLE:
      UserMetrics::RecordAction(
          UserMetricsAction("UpgradeCheck_UpgradeIsAvailable"), profile_);
      DCHECK(!google_updater_);  // Should have been nulled out already.
      google_updater_ = new GoogleUpdate();
      google_updater_->set_status_listener(this);
      UpdateStatus(UPGRADE_STARTED, GOOGLE_UPDATE_NO_ERROR);
      // CheckForUpdate(true,...) means perform upgrade if new version found.
      google_updater_->CheckForUpdate(true, window());
      // TODO(seanparent): Need to see if this code needs to change to
      // force a machine restart.
      return;
    case UPGRADE_ALREADY_UP_TO_DATE: {
      // The extra version check is necessary on Windows because the application
      // may be already up to date on disk though the running app is still
      // out of date. Chrome OS doesn't quite have this issue since the
      // OS/App are updated together. If a newer version of the OS has been
      // staged then UPGRADE_SUCESSFUL will be returned.
#if defined(OS_WIN)
      // Google Update reported that Chrome is up-to-date. Now make sure that we
      // are running the latest version and if not, notify the user by falling
      // into the next case of UPGRADE_SUCCESSFUL.
      BrowserDistribution* dist = BrowserDistribution::GetDistribution();
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      scoped_ptr<Version> installed_version(
          InstallUtil::GetChromeVersion(dist, false));
      if (!installed_version.get()) {
        // User-level Chrome is not installed, check system-level.
        installed_version.reset(InstallUtil::GetChromeVersion(dist, true));
      }
      scoped_ptr<Version> running_version(
          Version::GetVersionFromString(current_version_));
      if (!installed_version.get() ||
          (installed_version->CompareTo(*running_version) <= 0)) {
#endif
        UserMetrics::RecordAction(
            UserMetricsAction("UpgradeCheck_AlreadyUpToDate"), profile_);
#if defined(OS_CHROMEOS)
        std::wstring update_label_text = UTF16ToWide(l10n_util::GetStringFUTF16(
            IDS_UPGRADE_ALREADY_UP_TO_DATE,
            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
#else
        std::wstring update_label_text = l10n_util::GetStringFUTF16(
            IDS_UPGRADE_ALREADY_UP_TO_DATE,
            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
            ASCIIToUTF16(current_version_));
#endif
        if (base::i18n::IsRTL()) {
          update_label_text.push_back(
              static_cast<wchar_t>(base::i18n::kLeftToRightMark));
        }
        update_label_.SetText(update_label_text);
        show_success_indicator = true;
        break;
#if defined(OS_WIN)
      }
#endif
      // No break here as we want to notify user about upgrade if there is one.
    }
    case UPGRADE_SUCCESSFUL: {
      if (result == UPGRADE_ALREADY_UP_TO_DATE)
        UserMetrics::RecordAction(
            UserMetricsAction("UpgradeCheck_AlreadyUpgraded"), profile_);
      else
        UserMetrics::RecordAction(UserMetricsAction("UpgradeCheck_Upgraded"),
                                  profile_);
      restart_button_visible_ = true;
      const std::wstring& update_string =
          UTF16ToWide(l10n_util::GetStringFUTF16(
              IDS_UPGRADE_SUCCESSFUL_RESTART,
              l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
      update_label_.SetText(update_string);
      show_success_indicator = true;
      break;
    }
    case UPGRADE_ERROR:
      UserMetrics::RecordAction(UserMetricsAction("UpgradeCheck_Error"),
                                profile_);
      restart_button_visible_ = false;
      update_label_.SetText(UTF16ToWide(
          l10n_util::GetStringFUTF16Int(IDS_UPGRADE_ERROR, error_code)));
      show_timeout_indicator = true;
      break;
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
  View* parent = GetParent();
  parent->Layout();

  // Check button may have appeared/disappeared. We cannot call this during
  // ViewHierarchyChanged because the |window()| pointer hasn't been set yet.
  if (window())
    GetDialogClientView()->UpdateDialogButtons();
}

#endif
