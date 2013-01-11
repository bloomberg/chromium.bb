// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/script_bubble_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/extensions/script_bubble_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using extensions::Extension;
using extensions::ExtensionSystem;
using extensions::ScriptBubbleController;

namespace {

// Layout constants.
const int kInsetTop = 7;
const int kInsetLeft = 14;
const int kInsetBottom = 11;
const int kPaddingBelowHeadline = 12;
const int kPaddingRightOfIcon = 8;
const int kPaddingBetweenRows = 13;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ScriptBubbleView

ScriptBubbleView::ScriptEntry::ScriptEntry()
    : extension_imageview(NULL) {}

ScriptBubbleView::ScriptBubbleView(views::View* anchor_view,
                                   WebContents* web_contents)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      height_(0),
      web_contents_(web_contents) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_insets(gfx::Insets(5, 0, 5, 0));

  extensions::ScriptBubbleController* script_bubble_controller =
      GetScriptBubbleController();
  const std::set<std::string>& extensions_running_scripts =
      script_bubble_controller->extensions_running_scripts();
  Profile* profile = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext());
  ExtensionService* extension_service =
      ExtensionSystem::Get(profile)->extension_service();

  size_t i = 0;
  for (std::set<std::string>::const_iterator iter =
       extensions_running_scripts.begin();
       iter != extensions_running_scripts.end(); ++iter, ++i) {
    // We specify |true| to get disabled extensions as well, since content
    // scripts survive their extensions being disabled.
    const Extension* extension =
        extension_service->GetExtensionById(*iter, true);
    ScriptEntry entry;
    entry.extension_id = *iter;
    entry.extension_name = UTF8ToUTF16(extension->name());
    entries_.push_back(entry);

    int size = extension_misc::EXTENSION_ICON_BITTY;
    ExtensionResource image =
        extension->GetIconResource(size,
                                   ExtensionIconSet::MATCH_BIGGER);
    extensions::ImageLoader::Get(profile)->LoadImageAsync(
        extension, image, gfx::Size(size, size),
        base::Bind(&ScriptBubbleView::OnImageLoaded, AsWeakPtr(), i));
  }
}

ScriptBubbleView::~ScriptBubbleView() {
}

gfx::Size ScriptBubbleView::GetPreferredSize() {
  gfx::Size size(views::Widget::GetLocalizedContentsSize(
      IDS_SCRIPTBUBBLE_WIDTH_CHARS,
      IDS_SCRIPTBUBBLE_HEIGHT_LINES));
  size.set_height(std::max(size.height(), height_));
  return size;
}

void ScriptBubbleView::LinkClicked(views::Link* source, int event_flags) {
  std::string link(chrome::kChromeUIExtensionsURL);
  link += std::string("?id=") +
          entries_[source->id()].extension_id;
  web_contents_->OpenURL(OpenURLParams(GURL(link),
                                       Referrer(),
                                       NEW_FOREGROUND_TAB,
                                       content::PAGE_TRANSITION_LINK,
                                       false));
}

void ScriptBubbleView::Init() {
  height_ = 0;

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  layout->SetInsets(kInsetTop, kInsetLeft, 0, 0);
  height_ = kInsetTop + kInsetBottom;

  // Add a column for the heading (one large text column).
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING,   // Horizontal resize.
                     views::GridLayout::FILL,      // Vertical resize.
                     1,                            // Resize weight.
                     views::GridLayout::USE_PREF,  // Size type.
                     0,                            // Ignored for USE_PREF.
                     0);                           // Minimum size.

  // Add a column set for the extension image plus name.
  columns = layout->AddColumnSet(1);
  columns->AddColumn(views::GridLayout::LEADING,   // Horizontal resize.
                     views::GridLayout::FILL,      // Vertical resize.
                     0,                            // Resize weight.
                     views::GridLayout::USE_PREF,  // Size type.
                     0,                            // Ignored for USE_PREF.
                     0);                           // Minimum size.

  columns->AddPaddingColumn(0,                     // resize_percent.
                            kPaddingRightOfIcon);  // width.

  columns->AddColumn(views::GridLayout::LEADING,   // Horizontal resize.
                     views::GridLayout::FILL,      // Vertical resize.
                     1,                            // Resize weight.
                     views::GridLayout::USE_PREF,  // Size type.
                     0,                            // Ignored for USE_PREF.
                     0);                           // Minimum size.

  layout->StartRow(0, 0);
  views::Label* heading = new views::Label(
      l10n_util::GetStringUTF16(IDS_SCRIPT_BUBBLE_HEADLINE));
  layout->AddView(heading);
  height_ += heading->GetPreferredSize().height();

  layout->AddPaddingRow(0, kPaddingBelowHeadline);
  height_ += kPaddingBelowHeadline;

  for (size_t i = 0; i < entries_.size(); ++i) {
    layout->StartRow(0, 1);

    views::ImageView* image_view = new views::ImageView();
    entries_[i].extension_imageview = image_view;
    image_view->SetImageSize(gfx::Size(16, 16));
    image_view->SetImage(Extension::GetDefaultIcon(false));
    layout->AddView(image_view);

    views::Link* link = new views::Link(entries_[i].extension_name);
    link->set_id(i);
    link->set_listener(this);
    layout->AddView(link);

    height_ += std::max(image_view->GetPreferredSize().height(),
                        link->GetPreferredSize().height());

    if (i + 1 < entries_.size()) {
      layout->AddPaddingRow(0, kPaddingBetweenRows);
      height_ += kPaddingBetweenRows;
    }
  }

  layout->Layout(this);
}

void ScriptBubbleView::OnImageLoaded(size_t index,
                                     const gfx::Image& image) {
  if (!image.IsEmpty()) {
    const gfx::ImageSkia* image_skia = image.ToImageSkia();
    entries_[index].extension_imageview->SetImage(image_skia);
  }
}

ScriptBubbleController* ScriptBubbleView::GetScriptBubbleController() {
  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(web_contents_);
  return extensions_tab_helper->script_bubble_controller();
}
