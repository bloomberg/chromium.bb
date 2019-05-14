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
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
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

    /** Possible priorities for this set of properties. */
    @IntDef({Priority.HIGH, Priority.MEDIUM, Priority.LOW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Priority {
        int HIGH = 0;
        int MEDIUM = 1;
        int LOW = 2;
    }

    /** Properties that determine how list items are displayed in the touchless dialog. */
    public static class DialogListItemProperties {
        /** The icon of the dialog. */
        public static final WritableObjectPropertyKey<Drawable> ICON =
                new WritableObjectPropertyKey<>();

        /** The text shown for the list item. */
        public static final WritableObjectPropertyKey<String> TEXT =
                new WritableObjectPropertyKey<>();

        /** Whether this item can be clicked more than one time. */
        public static final WritableBooleanPropertyKey MULTI_CLICKABLE =
                new WritableBooleanPropertyKey();

        /** The action to be performed when the item is selected. */
        public static final WritableObjectPropertyKey<OnClickListener> CLICK_LISTENER =
                new WritableObjectPropertyKey<>();

        /** Whether this item has a focus change listener attached to its view. */
        public static final WritableBooleanPropertyKey FOCUS_LISTENER_SET =
                new WritableBooleanPropertyKey();

        public static final PropertyKey[] ALL_KEYS = {
                ICON, TEXT, MULTI_CLICKABLE, CLICK_LISTENER, FOCUS_LISTENER_SET};
    }

    /**
     * Struct-like class for holding the Names for the dialog actions.
     */
    public static class ActionNames {
        public String cancel;
        public String select;
        public String alt;
    }

    /**
     * Whether the dialog is fullscreen. If false there will be a gap at the top showing the content
     * behind the dialog.
     */
    public static final ReadableBooleanPropertyKey IS_FULLSCREEN = new ReadableBooleanPropertyKey();

    /**
     * The list of options to be displayed in the dialog. These models should be using the
     * {@link DialogListItemProperties} properties.
     */
    public static final WritableObjectPropertyKey<PropertyModel[]> LIST_MODELS =
            new WritableObjectPropertyKey<>();

    /** The names for the Actions. */
    public static final WritableObjectPropertyKey<ActionNames> ACTION_NAMES =
            new WritableObjectPropertyKey<>();

    /** What will happen when cancel is triggered. */
    public static final WritableObjectPropertyKey<OnClickListener> CANCEL_ACTION =
            new WritableObjectPropertyKey<>();

    /** What will happen when alternative action is triggered. */
    public static final WritableObjectPropertyKey<OnClickListener> ALT_ACTION =
            new WritableObjectPropertyKey<>();

    /** The priority for this set of properties. */
    public static final WritableIntPropertyKey PRIORITY = new WritableIntPropertyKey();

    public static final PropertyKey[] MINIMAL_DIALOG_KEYS = {
            ModalDialogProperties.TITLE, ACTION_NAMES, CANCEL_ACTION, ALT_ACTION, PRIORITY};

    public static final PropertyKey[] ALL_DIALOG_KEYS =
            PropertyModel.concatKeys(ModalDialogProperties.ALL_KEYS,
                    new PropertyKey[] {ACTION_NAMES, CANCEL_ACTION, ALT_ACTION, PRIORITY,
                            IS_FULLSCREEN, LIST_MODELS});
}
