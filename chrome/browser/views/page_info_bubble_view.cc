// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/page_info_bubble_view.h"

#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/browser/views/toolbar_view.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/separator.h"
#include "views/grid_layout.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

// Layout constants.
const int kHGapToBorder = 11;
const int kVGapToImage = 10;
const int kVGapToHeadline = 7;
const int kHGapImageToDescription = 6;
const int kTextPaddingRight = 10;
const int kPaddingBelowSeparator = 4;
const int kPaddingAboveSeparator = 13;
const int kIconOffset = 28;

// A section contains an image that shows a status (good or bad), a title, an
// optional head-line (in bold) and a description.
class Section : public views::View {
 public:
  Section(bool state,
          const string16& headline,
          const string16& description);
  virtual ~Section();

  virtual int GetHeightForWidth(int w);
  virtual void Layout();

 private:
  // Calculate the layout if |compute_bounds_only|, otherwise does Layout also.
  gfx::Size LayoutItems(bool compute_bounds_only, int width);

  // Whether to show the good/bad icon.
  bool state_;

  // The first line of the description, show in bold.
  string16 headline_;

  // The description, displayed below the head line.
  string16 description_;

  static SkBitmap* good_state_icon_;
  static SkBitmap* bad_state_icon_;

  views::ImageView* status_image_;
  views::Label* headline_label_;
  views::Label* description_label_;

  DISALLOW_COPY_AND_ASSIGN(Section);
};

// static
SkBitmap* Section::good_state_icon_ = NULL;
SkBitmap* Section::bad_state_icon_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// PageInfoBubbleView

PageInfoBubbleView::PageInfoBubbleView(Profile* profile,
                                       const GURL& url,
                                       const NavigationEntry::SSLStatus& ssl,
                                       bool show_history)
    : ALLOW_THIS_IN_INITIALIZER_LIST(model_(profile, url, ssl,
                                            show_history, this)),
      cert_id_(ssl.cert_id()),
      info_bubble_(NULL) {
  LayoutSections();
}

PageInfoBubbleView::~PageInfoBubbleView() {
}

void PageInfoBubbleView::LayoutSections() {
  // Remove all the existing sections.
  RemoveAllChildViews(true);

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL,  // Horizontal resize.
                     views::GridLayout::FILL,  // Vertical resize.
                     1,   // Resize weight.
                     views::GridLayout::USE_PREF,  // Size type.
                     0,   // Ignored for USE_PREF.
                     0);  // Minimum size.

  int count = model_.GetSectionCount();
  for (int i = 0; i < count; ++i) {
    PageInfoModel::SectionInfo info = model_.GetSectionInfo(i);
    layout->StartRow(0, 0);
    // TODO(finnur): Remove title from the info struct, since it is
    //               not used anymore.
    layout->AddView(new Section(info.state, info.head_line, info.description));

    // Add separator after all sections except the last.
    if (i < count - 1) {
      layout->AddPaddingRow(0, kPaddingAboveSeparator);
      layout->StartRow(0, 0);
      layout->AddView(new views::Separator());
      layout->AddPaddingRow(0, kPaddingBelowSeparator);
    }
  }
}

gfx::Size PageInfoBubbleView::GetPreferredSize() {
  gfx::Size size(views::Window::GetLocalizedContentsSize(
      IDS_PAGEINFOBUBBLE_WIDTH_CHARS, IDS_PAGEINFOBUBBLE_HEIGHT_LINES));
  size.set_height(0);

  int count = model_.GetSectionCount();
  for (int i = 0; i < count; ++i) {
    PageInfoModel::SectionInfo info = model_.GetSectionInfo(i);
    Section section(info.state, info.head_line, info.description);
    size.Enlarge(0, section.GetHeightForWidth(size.width()));
  }

  // Account for the separators and padding.
  views::Separator separator;
  gfx::Size separator_size = separator.GetPreferredSize();
  size.Enlarge(0, (count - 1) * (separator_size.height() +
                                 kPaddingAboveSeparator +
                                 kPaddingBelowSeparator));
  return size;
}

void PageInfoBubbleView::ModelChanged() {
  LayoutSections();
  info_bubble_->SizeToContents();
}

////////////////////////////////////////////////////////////////////////////////
// Section

Section::Section(bool state,
                 const string16& headline,
                 const string16& description)
    : state_(state),
      headline_(headline),
      description_(description) {
  if (!good_state_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    good_state_icon_ = rb.GetBitmapNamed(IDR_PAGEINFO_GOOD);
    bad_state_icon_ = rb.GetBitmapNamed(IDR_PAGEINFO_BAD);
  }

  status_image_ = new views::ImageView();
  status_image_->SetImage(state ? good_state_icon_ : bad_state_icon_);
  AddChildView(status_image_);

  headline_label_ = new views::Label(UTF16ToWideHack(headline));
  headline_label_->SetFont(
      headline_label_->font().DeriveFont(0, gfx::Font::BOLD));
  headline_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(headline_label_);

  description_label_ = new views::Label(UTF16ToWideHack(description));
  description_label_->SetMultiLine(true);
  description_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  // Allow linebreaking in the middle of words if necessary, so that extremely
  // long hostnames (longer than one line) will still be completely shown.
  description_label_->SetAllowCharacterBreak(true);
  AddChildView(description_label_);
}

Section::~Section() {
}

int Section::GetHeightForWidth(int width) {
  return LayoutItems(true, width).height();
}

void Section::Layout() {
  LayoutItems(false, width());
}

gfx::Size Section::LayoutItems(bool compute_bounds_only, int width) {
  int x = kHGapToBorder;
  int y = kVGapToImage;

  // Layout the image, head-line and description.
  gfx::Size size = status_image_->GetPreferredSize();
  if (!compute_bounds_only)
    status_image_->SetBounds(x, y, size.width(), size.height());
  int image_height = y + size.height();
  x += size.width() + kHGapImageToDescription;
  int w = width - x - kTextPaddingRight;
  y = kVGapToHeadline;
  if (!headline_label_->GetText().empty()) {
    size = headline_label_->GetPreferredSize();
    if (!compute_bounds_only)
      headline_label_->SetBounds(x, y, w > 0 ? w : 0, size.height());
    y += size.height();
  } else {
    if (!compute_bounds_only)
      headline_label_->SetBounds(x, y, 0, 0);
  }
  if (w > 0) {
    int height = description_label_->GetHeightForWidth(w);
    if (!compute_bounds_only)
      description_label_->SetBounds(x, y, w, height);
    y += height;
  } else {
    if (!compute_bounds_only)
      description_label_->SetBounds(x, y, 0, 0);
  }

  // Make sure the image is not truncated if the text doesn't contain much.
  y = std::max(y, image_height);
  return gfx::Size(width, y);
}

namespace browser {

void ShowPageInfoBubble(gfx::NativeWindow parent,
                        Profile* profile,
                        const GURL& url,
                        const NavigationEntry::SSLStatus& ssl,
                        bool show_history) {
  // Find where to point the bubble at.
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(parent);
  gfx::Rect bounds = browser_view->toolbar()->location_bar()->bounds();
  gfx::Point point;
  views::View::ConvertPointToScreen(browser_view->toolbar()->location_bar(),
                                    &point);
  bounds.set_origin(point);
  bounds.set_width(kIconOffset);

  // Show the bubble.
  PageInfoBubbleView* page_info_bubble =
      new PageInfoBubbleView(profile, url, ssl, show_history);
  InfoBubble* info_bubble =
      InfoBubble::Show(browser_view->GetWidget(), bounds,
                       BubbleBorder::TOP_LEFT,
                       page_info_bubble, page_info_bubble);
  page_info_bubble->set_info_bubble(info_bubble);
}

}
