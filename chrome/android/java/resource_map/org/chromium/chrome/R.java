// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome;

/**
 * Provide Android internal resources to Chrome's chrome layer.  This allows
 * classes that access resources via org.chromium.chrome.R to function properly
 * in webview.  In a normal Chrome build, chrome resources live in a res folder
 * in the chrome layer and the org.chromium.chrome.R class is generated at
 * build time based on these resources.  In webview, resources live in the
 * Android framework and can't be accessed directly from the chrome layer.
 * Instead, we copy resources needed by chrome into the Android framework and
 * use this R class to map resources IDs from org.chromium.chrome.R to
 * com.android.internal.R.
 */
public final class R {
    public static final class color {
        public static int js_modal_dialog_title_separator;
    }
    public static final class dimen {
        public static int favicon_colorstrip_corner_radii;
        public static int favicon_colorstrip_height;
        public static int favicon_colorstrip_padding;
        public static int favicon_colorstrip_width;
        public static int favicon_fold_border;
        public static int favicon_fold_corner_radii;
        public static int favicon_fold_shadow;
        public static int favicon_fold_size;
        public static int favicon_size;
    }
    public static final class drawable {
        public static int globe_favicon;
    }
    public static final class id {
        public static int autofill_label;
        public static int autofill_name;
        public static int js_modal_dialog_button_cancel;
        public static int js_modal_dialog_button_confirm;
        public static int js_modal_dialog_message;
        public static int js_modal_dialog_prompt;
        public static int js_modal_dialog_title;
        public static int suppress_js_modal_dialogs;
    }
    public static final class layout {
        public static int autofill_text;
        public static int js_modal_dialog;
    }
    public static final class mipmap {
        public static int homescreen_bg;
        public static int icon;
    }
    public static final class string {
        public static int accessibility_js_modal_dialog_prompt;
        public static int dont_reload_this_page;
        public static int js_modal_dialog_cancel;
        public static int js_modal_dialog_confirm;
        public static int leave_this_page;
        public static int reload_this_page;
        public static int stay_on_this_page;
        public static int suppress_js_modal_dialogs;
    }
}
