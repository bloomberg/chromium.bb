// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.dialog;

import android.graphics.drawable.Drawable;
import android.support.annotation.IntDef;
import android.view.View.OnClickListener;

import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The properties that define a touchless dialog. */
public class TouchlessDialogProperties {
    /** Possible list item types for the list of options in the dialog. */
    @IntDef({ListItemType.DEFAULT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ListItemType {
        int DEFAULT = 0;
    }

    /** Properties that determine how list items are displayed in the touchless dialog. */
    public static class DialogListItemProperties {
        /** The icon of the dialog. */
        public static final WritableObjectPropertyKey<Drawable> ICON =
                new WritableObjectPropertyKey<>();

        /** The text shown for the list item. */
        public static final WritableObjectPropertyKey<String> TEXT =
                new WritableObjectPropertyKey<>();

        /** The action to be performed when the item is selected. */
        public static final WritableObjectPropertyKey<OnClickListener> CLICK_LISTENER =
                new WritableObjectPropertyKey<>();

        public static final PropertyKey[] ALL_KEYS = {ICON, TEXT, CLICK_LISTENER};
    }

    /**
     * Whether the dialog is fullscreen. If false there will be a gap at the top showing the content
     * behind the dialog.
     */
    public static final ReadableBooleanPropertyKey IS_FULLSCREEN = new ReadableBooleanPropertyKey();

    /** The title of the dialog. */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    /**
     * The list of options to be displayed in the dialog. These models should be using the
     * {@link DialogListItemProperties} properties.
     */
    public static final WritableObjectPropertyKey<PropertyModel[]> LIST_MODELS =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {IS_FULLSCREEN, TITLE, LIST_MODELS};

    public static final PropertyKey[] ALL_DIALOG_KEYS =
            PropertyModel.concatKeys(ModalDialogProperties.ALL_KEYS, ALL_KEYS);
}
