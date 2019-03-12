// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.R;

/**
 * Hacky class to avoid lint errors for resources called on the module.
 */
/* package */ class SilenceLintErrors {
    // TODO(yusufo): Add these resources to the DFM
    private int[] mRes =
            new int[] {R.dimen.tab_grid_favicon_size, R.string.tab_management_module_title,
                    R.string.iph_tab_groups_quickly_compare_pages_text,
                    R.string.iph_tab_groups_tap_to_see_another_tab_text,
                    R.string.iph_tab_groups_your_tabs_together_text,
                    R.string.bottom_tab_grid_description, R.string.bottom_tab_grid_opened_half,
                    R.string.bottom_tab_grid_opened_full, R.string.bottom_tab_grid_closed,
                    R.dimen.tab_list_selected_inset, R.layout.tab_strip_item,
                    R.drawable.selected_tab_background, R.drawable.tab_grid_card_background,
                    R.layout.tab_grid_card_item, R.layout.tab_list_recycler_view_layout,
                    R.layout.bottom_tab_grid_toolbar, R.string.bottom_tab_grid_new_tab,
                    R.string.bottom_tab_grid_new_tab, R.plurals.bottom_tab_grid_title_placeholder,
                    R.string.iph_tab_groups_tap_to_see_another_tab_accessibility_text,
                    R.string.accessibility_bottom_tab_strip_expand_tab_sheet,
                    R.layout.bottom_tab_strip_toolbar, R.drawable.tabstrip_selected};

    private SilenceLintErrors() {}
}
