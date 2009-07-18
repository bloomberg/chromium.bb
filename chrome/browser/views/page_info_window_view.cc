// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <cryptuiapi.h>
#pragma comment(lib, "cryptui.lib")
#endif

#include "app/resource_bundle.h"
#include "app/l10n_util.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/page_info_model.h"
#include "chrome/common/pref_names.h"
#include "grit/locale_settings.h"
#include "grit/generated_resources.h"
#include "net/base/x509_certificate.h"
#include "views/background.h"
#include "views/grid_layout.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/separator.h"
#include "views/standard_layout.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

namespace {

// Layout constants.
const int kHGapToBorder = 6;
const int kHGapTitleToSeparator = 2;
const int kVGapTitleToImage = 6;
const int kHGapImageToDescription = 6;
const int kVGapHeadLineToDescription = 2;
const int kHExtraSeparatorPadding = 2;
const int kHorizontalPadding = 10;
const int kVerticalPadding = 20;

class PageInfoWindowView : public views::View,
                           public views::DialogDelegate,
                           public views::ButtonListener,
                           public PageInfoModel::PageInfoModelObserver {
 public:
  PageInfoWindowView(gfx::NativeView parent,
                     Profile* profile,
                     const GURL& url,
                     const NavigationEntry::SSLStatus& ssl,
                     bool show_history);
  virtual ~PageInfoWindowView();

  // This is the main initializer that creates the window.
  virtual void Init(gfx::NativeView parent);

  // views::View overrides:
  virtual gfx::Size GetPreferredSize();

  // views::Window overridden method.
  virtual void Show();

  virtual void ShowCertDialog(int cert_id);

  // views::ButtonListener method.
  virtual void ButtonPressed(views::Button* sender);

  // views::DialogDelegate methods:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetWindowName() const;
  virtual views::View* GetContentsView();
  virtual views::View* GetExtraView();
  virtual bool CanResize() const { return true; }

  // PageInfoModel::PageInfoModelObserver method.
  virtual void ModelChanged();

 private:
 // This retreives the sections from the model and lay them out.
  void LayoutSections();

  // Offsets the specified rectangle so it is showing on the screen and shifted
  // from its original location.
  void CalculateWindowBounds(gfx::Rect* bounds);

  views::NativeButton* cert_info_button_;

  // The model providing the various section info.
  PageInfoModel model_;

  // The id of the certificate for this page.
  int cert_id_;

  // A counter of how many page info windows are currently opened.
  static int opened_window_count_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoWindowView);
};

// A section contains an image that shows a status (good or bad), a title, an
// optional head-line (in bold) and a description.
class Section : public views::View {
 public:
  Section(const std::wstring& title,
          bool state,
          const std::wstring& head_line,
          const std::wstring& description);
  virtual ~Section();

  virtual int GetHeightForWidth(int w);
  virtual void Layout();

 private:
  // The text placed on top of the section (on the left of the separator bar).
  std::wstring title_;

  // Whether to show the good/bad icon.
  bool state_;

  // The first line of the description, show in bold.
  std::wstring head_line_;

  // The description, displayed below the head line.
  std::wstring description_;

  static SkBitmap* good_state_icon_;
  static SkBitmap* bad_state_icon_;

  views::Label* title_label_;
  views::Separator* separator_;
  views::ImageView* status_image_;
  views::Label* head_line_label_;
  views::Label* description_label_;

  DISALLOW_COPY_AND_ASSIGN(Section);
};

// static
SkBitmap* Section::good_state_icon_ = NULL;
SkBitmap* Section::bad_state_icon_ = NULL;
int PageInfoWindowView::opened_window_count_ = 0;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PageInfoWindowViews

PageInfoWindowView::PageInfoWindowView(gfx::NativeView parent,
                                       Profile* profile,
                                       const GURL& url,
                                       const NavigationEntry::SSLStatus& ssl,
                                       bool show_history)
    : model_(profile, url, ssl, show_history, this),
      cert_id_(ssl.cert_id()) {
  Init(parent);
}

PageInfoWindowView::~PageInfoWindowView() {
  DCHECK(opened_window_count_ > 0);
  opened_window_count_--;
}

void PageInfoWindowView::Init(gfx::NativeView parent) {
  DWORD sys_color = ::GetSysColor(COLOR_3DFACE);
  SkColor color = SkColorSetRGB(GetRValue(sys_color), GetGValue(sys_color),
                                GetBValue(sys_color));
  set_background(views::Background::CreateSolidBackground(color));

  LayoutSections();

  if (opened_window_count_ > 0) {
    // There already is a PageInfo window opened.  Let's shift the location of
    // the new PageInfo window so they don't overlap entirely.
    // Window::Init will position the window from the stored location.
    gfx::Rect bounds;
    bool maximized = false;
    if (GetSavedWindowBounds(&bounds) && GetSavedMaximizedState(&maximized)) {
      CalculateWindowBounds(&bounds);
      SaveWindowPlacement(bounds, maximized);
    }
  }

  views::Window::CreateChromeWindow(parent, gfx::Rect(), this);
}

gfx::Size PageInfoWindowView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_PAGEINFO_DIALOG_WIDTH_CHARS, IDS_PAGEINFO_DIALOG_HEIGHT_LINES));
}

void PageInfoWindowView::LayoutSections() {
  // Remove all the existing sections.
  RemoveAllChildViews(true);

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddPaddingColumn(0, kHorizontalPadding);
  columns->AddColumn(views::GridLayout::FILL,  // Horizontal resize.
                     views::GridLayout::FILL,  // Vertical resize.
                     1,  // Resize weight.
                     views::GridLayout::USE_PREF,  // Size type.
                     0,  // Ignored for USE_PREF.
                     0);  // Minimum size.
  columns->AddPaddingColumn(0, kHorizontalPadding);

  layout->AddPaddingRow(0, kVerticalPadding);
  for (int i = 0; i < model_.GetSectionCount(); ++i) {
    PageInfoModel::SectionInfo info = model_.GetSectionInfo(i);
    layout->StartRow(0, 0);
    layout->AddView(new Section(info.title, info.state, info.head_line,
                                info.description));
    layout->AddPaddingRow(0, kVerticalPadding);
  }
  layout->AddPaddingRow(1, kVerticalPadding);

  Layout();
}

void PageInfoWindowView::Show() {
  window()->Show();
  opened_window_count_++;
}

int PageInfoWindowView::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

std::wstring PageInfoWindowView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_PAGEINFO_WINDOW_TITLE);
}

std::wstring PageInfoWindowView::GetWindowName() const {
  return prefs::kPageInfoWindowPlacement;
}

views::View* PageInfoWindowView::GetContentsView() {
  return this;
}

views::View* PageInfoWindowView::GetExtraView() {
  if (!cert_id_)
    return NULL;
  scoped_refptr<net::X509Certificate> cert;
  CertStore::GetSharedInstance()->RetrieveCert(cert_id_, &cert);
  // When running with Gears, we have no os certificate, so there is no cert
  // to show. Don't bother showing the cert info button in that case.
  if (!cert.get() || !cert->os_cert_handle())
    return NULL;

  // The dialog sizes the extra view to fill the entire available space.
  // We use a container to layout it out properly.
  views::View* button_container = new views::View();
  views::GridLayout* layout = new views::GridLayout(button_container);
  button_container->SetLayoutManager(layout);

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddPaddingColumn(0, kHorizontalPadding);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(new views::NativeButton(this,
      l10n_util::GetString(IDS_PAGEINFO_CERT_INFO_BUTTON)));

  return button_container;
}

void PageInfoWindowView::ModelChanged() {
  LayoutSections();
}

void PageInfoWindowView::ButtonPressed(views::Button* sender) {
  // So far we only listen for the "Certificate info" button.
  DCHECK(cert_id_ != 0);
  ShowCertDialog(cert_id_);
}

void PageInfoWindowView::CalculateWindowBounds(gfx::Rect* bounds) {
  const int kDefaultOffset = 15;

  gfx::Rect monitor_bounds(win_util::GetMonitorBoundsForRect(*bounds));
  if (monitor_bounds.IsEmpty())
    return;

  // If necessary, move the window so it is visible on the screen.
  gfx::Rect adjusted_bounds = bounds->AdjustToFit(monitor_bounds);
  if (adjusted_bounds != *bounds) {
    // The bounds have moved, we are done.
    *bounds = adjusted_bounds;
    return;
  }

  // Move the window from its specified position, trying to keep it entirely
  // visible.
  int x_offset, y_offset;
  if (bounds->right() + kDefaultOffset >= monitor_bounds.right() &&
      abs(monitor_bounds.x() - bounds->x()) >= kDefaultOffset) {
    x_offset = -kDefaultOffset;
  } else {
    x_offset = kDefaultOffset;
  }

  if (bounds->bottom() + kDefaultOffset >= monitor_bounds.bottom() &&
      abs(monitor_bounds.y() - bounds->y()) >= kDefaultOffset) {
    y_offset = -kDefaultOffset;
  } else {
    y_offset = kDefaultOffset;
  }

  bounds->Offset(x_offset, y_offset);
}

void PageInfoWindowView::ShowCertDialog(int cert_id) {
#if defined(OS_WIN)
  scoped_refptr<net::X509Certificate> cert;
  CertStore::GetSharedInstance()->RetrieveCert(cert_id, &cert);
  if (!cert.get()) {
    // The certificate was not found. Could be that the renderer crashed before
    // we displayed the page info.
    return;
  }

  CRYPTUI_VIEWCERTIFICATE_STRUCT view_info = { 0 };
  view_info.dwSize = sizeof(view_info);
  // We set our parent to the tab window. This makes the cert dialog created
  // in CryptUIDlgViewCertificate modal to the browser.
  view_info.hwndParent = window()->GetNativeWindow();
  view_info.dwFlags = CRYPTUI_DISABLE_EDITPROPERTIES |
                      CRYPTUI_DISABLE_ADDTOSTORE;
  view_info.pCertContext = cert->os_cert_handle();
  // Search the cert store that 'cert' is in when building the cert chain.
  HCERTSTORE cert_store = view_info.pCertContext->hCertStore;
  view_info.cStores = 1;
  view_info.rghStores = &cert_store;
  BOOL properties_changed;

  // This next call blocks but keeps processing windows messages, making it
  // modal to the browser window.
  BOOL rv = ::CryptUIDlgViewCertificate(&view_info, &properties_changed);
#else
  NOTIMPLEMENTED();
#endif
}

////////////////////////////////////////////////////////////////////////////////
// Section

Section::Section(const std::wstring& title,
                 bool state,
                 const std::wstring& head_line,
                 const std::wstring& description)
    : title_(title),
      state_(state),
      head_line_(head_line),
      description_(description) {
  if (!good_state_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    good_state_icon_ = rb.GetBitmapNamed(IDR_PAGEINFO_GOOD);
    bad_state_icon_ = rb.GetBitmapNamed(IDR_PAGEINFO_BAD);
  }
  title_label_ = new views::Label(title);
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(title_label_);

  separator_ = new views::Separator();
  AddChildView(separator_);

  status_image_ = new views::ImageView();
  status_image_->SetImage(state ? good_state_icon_ : bad_state_icon_);
  AddChildView(status_image_);

  head_line_label_ = new views::Label(head_line);
  head_line_label_->SetFont(
      head_line_label_->GetFont().DeriveFont(0, gfx::Font::BOLD));
  head_line_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(head_line_label_);

  description_label_ = new views::Label(description);
  description_label_->SetMultiLine(true);
  description_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(description_label_);
}

Section::~Section() {
}

int Section::GetHeightForWidth(int width) {
  // The height of the section depends on the height of the description label
  // (multi-line).  We need to know the width of the description label to know
  // its height.
  int height = 0;
  gfx::Size size = title_label_->GetPreferredSize();
  height += size.height() + kVGapTitleToImage;

  gfx::Size image_size = status_image_->GetPreferredSize();

  int text_height = 0;
  if (!head_line_label_->GetText().empty()) {
    size = head_line_label_->GetPreferredSize();
    text_height = size.height() + kVGapHeadLineToDescription;
  }

  int description_width =
      width - image_size.width() - kHGapImageToDescription - kHGapToBorder;
  text_height += description_label_->GetHeightForWidth(description_width);

  height += std::max(image_size.height(), text_height);

  return height;
}

void Section::Layout() {
  // First, layout the title and separator.
  int x = 0;
  int y = 0;
  gfx::Size size = title_label_->GetPreferredSize();
  title_label_->SetBounds(x, y, size.width(), size.height());
  x += size.width() + kHGapTitleToSeparator;
  separator_->SetBounds(x + kHExtraSeparatorPadding, y,
                        width() - x - 2 * kHExtraSeparatorPadding,
                        size.height());

  // Then the image, head-line and description.
  x = kHGapToBorder;
  y += title_label_->height() + kVGapTitleToImage;
  size = status_image_->GetPreferredSize();
  status_image_->SetBounds(x, y, size.width(), size.height());
  x += size.width() + kHGapImageToDescription;
  int w = width() - x;
  if (!head_line_label_->GetText().empty()) {
    size = head_line_label_->GetPreferredSize();
    head_line_label_->SetBounds(x, y, w > 0 ? w : 0, size.height());
    y += size.height() + kVGapHeadLineToDescription;
  } else {
    head_line_label_->SetBounds(x, y, 0, 0);
  }
  if (w > 0) {
    description_label_->SetBounds(x, y, w,
                                  description_label_->GetHeightForWidth(w));
  } else {
    description_label_->SetBounds(x, y, 0, 0);
  }
}

namespace browser {

void ShowPageInfo(gfx::NativeView parent,
                  Profile* profile,
                  const GURL& url,
                  const NavigationEntry::SSLStatus& ssl,
                  bool show_history) {
  PageInfoWindowView* page_info_window =
      new PageInfoWindowView(parent, profile, url, ssl, show_history);
  page_info_window->Show();
}

}
