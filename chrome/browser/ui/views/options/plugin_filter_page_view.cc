// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/plugin_filter_page_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/options/show_options_url.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/grid_layout.h"
#include "views/layout/layout_constants.h"

PluginFilterPageView::PluginFilterPageView(Profile* profile)
    : ContentFilterPageView(profile, CONTENT_SETTINGS_TYPE_PLUGINS) {
}

PluginFilterPageView::~PluginFilterPageView() {
}

///////////////////////////////////////////////////////////////////////////////
// PluginFilterPageView, ContentFilterPageView override:

void PluginFilterPageView::InitControlLayout() {
  ContentFilterPageView::InitControlLayout();

  using views::GridLayout;

  GridLayout* layout = static_cast<GridLayout*>(GetLayoutManager());
  const int single_column_set_id = 0;
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  views::Link* plugins_page_link = new views::Link(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_PLUGIN_SELECTIVE_DISABLE)));
  plugins_page_link->SetController(this);

  layout->StartRow(0, single_column_set_id);
  layout->AddView(plugins_page_link, 1, 1, GridLayout::LEADING,
                  GridLayout::FILL);
}

///////////////////////////////////////////////////////////////////////////////
// PluginFilterPageView, views::LinkController implementation:

void PluginFilterPageView::LinkActivated(views::Link* source,
                                         int event_flags) {
  browser::ShowOptionsURL(profile(), GURL(chrome::kChromeUIPluginsURL));
}
