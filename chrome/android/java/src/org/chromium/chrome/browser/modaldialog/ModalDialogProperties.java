// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * The model properties for a modal dialog.
 */
public class ModalDialogProperties {
    /** The {@link ModalDialogView.Controller} that handles events on user actions. */
    public static final ReadableObjectPropertyKey<ModalDialogView.Controller> CONTROLLER =
            new ReadableObjectPropertyKey<>();

    /** The content description of the dialog for accessibility. */
    public static final ReadableObjectPropertyKey<String> CONTENT_DESCRIPTION =
            new ReadableObjectPropertyKey<>();

    /** The title of the dialog. */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    /** The title icon of the dialog. */
    public static final WritableObjectPropertyKey<Drawable> TITLE_ICON =
            new WritableObjectPropertyKey<>();

    /** The message of the dialog. */
    public static final WritableObjectPropertyKey<String> MESSAGE =
            new WritableObjectPropertyKey<>();

    /** The customized content view of the dialog. */
    public static final WritableObjectPropertyKey<View> CUSTOM_VIEW =
            new WritableObjectPropertyKey<>();

    /** The text on the positive button. */
    public static final WritableObjectPropertyKey<String> POSITIVE_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /** The enabled state on the positive button. */
    public static final WritableBooleanPropertyKey POSITIVE_BUTTON_DISABLED =
            new WritableBooleanPropertyKey();

    /** The text on the negative button. */
    public static final WritableObjectPropertyKey<String> NEGATIVE_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /** The enabled state on the negative button. */
    public static final WritableBooleanPropertyKey NEGATIVE_BUTTON_DISABLED =
            new WritableBooleanPropertyKey();

    /** Whether the dialog should be dismissed on user tapping the scrim. */
    public static final WritableBooleanPropertyKey CANCEL_ON_TOUCH_OUTSIDE =
            new WritableBooleanPropertyKey();

    /** Whether the title is scrollable with the message. */
    public static final WritableBooleanPropertyKey TITLE_SCROLLABLE =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {CONTROLLER, CONTENT_DESCRIPTION,
            TITLE, TITLE_ICON, MESSAGE, CUSTOM_VIEW, POSITIVE_BUTTON_TEXT, POSITIVE_BUTTON_DISABLED,
            NEGATIVE_BUTTON_TEXT, NEGATIVE_BUTTON_DISABLED, CANCEL_ON_TOUCH_OUTSIDE,
            TITLE_SCROLLABLE};
}
