// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.support.v7.widget.Toolbar;
import android.util.AttributeSet;

import org.chromium.chrome.R;

/** Simple class for displaying a toolbar in the editor dialog. */
public class EditorDialogToolbar extends Toolbar {

    /** Constructor for when the toolbar is inflated from XML. */
    public EditorDialogToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        inflateMenu(R.menu.payments_editor_menu);
    }

}
