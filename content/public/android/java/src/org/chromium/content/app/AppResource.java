// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;

/**
 * Interface to list and expose any required Android app resources.
 * All resources must be registered before instantiating ContentView objects.
 */
public class AppResource {
    /** Array resource containing the official command line arguments. */
    public static int ARRAY_OFFICIAL_COMMAND_LINE;

    /** Dimension defining the corner radii of the favicon color strip when creating shortcuts. */
    public static int DIMENSION_FAVICON_COLORSTRIP_CORNER_RADII;

    /** Dimension defining the height of the favicon color strip when creating shortcuts. */
    public static int DIMENSION_FAVICON_COLORSTRIP_HEIGHT;

    /** Dimension defining the padding of the favicon color strip when creating shortcuts. */
    public static int DIMENSION_FAVICON_COLORSTRIP_PADDING;

    /** Dimension defining the width of the favicon color strip when creating shortcuts. */
    public static int DIMENSION_FAVICON_COLORSTRIP_WIDTH;

    /** Dimension defining the border of the favicon fold when creating shortcuts. */
    public static int DIMENSION_FAVICON_FOLD_BORDER;

    /** Dimension defining the corner radii of the favicon fold when creating shortcuts. */
    public static int DIMENSION_FAVICON_FOLD_CORNER_RADII;

    /** Dimension defining the shadow of the favicon fold when creating shortcuts. */
    public static int DIMENSION_FAVICON_FOLD_SHADOW;

    /** Dimension defining the size of the favicon fold when creating shortcuts. */
    public static int DIMENSION_FAVICON_FOLD_SIZE;

    /** Dimension defining the size of the favicon image when creating shortcuts. */
    public static int DIMENSION_FAVICON_SIZE;

    /** Dimension of the radius used in the link preview overlay. */
    public static int DIMENSION_LINK_PREVIEW_OVERLAY_RADIUS;

    /** Drawable icon resource for the Share button in the action bar. */
    public static int DRAWABLE_ICON_ACTION_BAR_SHARE;

    /** Drawable icon resource for the Web Search button in the action bar. */
    public static int DRAWABLE_ICON_ACTION_BAR_WEB_SEARCH;

    /** Drawable icon resource of the Application icon. */
    public static int DRAWABLE_ICON_APP_ICON;

    /** Drawable icon resource used for favicons by default. */
    public static int DRAWABLE_ICON_DEFAULT_FAVICON;

    /** Drawable resource for the link preview popup overlay. */
    public static int DRAWABLE_LINK_PREVIEW_POPUP_OVERLAY;

    /** Id of the date picker view. */
    public static int ID_DATE_PICKER;

    /** Id of the cancel button in Javascript modal dialogs. */
    public static int ID_JS_MODAL_DIALOG_BUTTON_CANCEL;

    /** Id of the confirm button in Javascript modal dialogs. */
    public static int ID_JS_MODAL_DIALOG_BUTTON_CONFIRM;

    /** Id of the CheckBox to suppress further modal dialogs in Javascript modal dialogs. */
    public static int ID_JS_MODAL_DIALOG_CHECKBOX_SUPPRESS_DIALOGS;

    /** Id of the message TextView in Javascript modal dialogs. */
    public static int ID_JS_MODAL_DIALOG_TEXT_MESSAGE;

    /** Id of the prompt EditText in Javascript modal dialogs. */
    public static int ID_JS_MODAL_DIALOG_TEXT_PROMPT;

    /** Id of the title TextView in Javascript modal dialogs. */
    public static int ID_JS_MODAL_DIALOG_TEXT_TITLE;

    /** Id of the month picker view. */
    public static int ID_MONTH_PICKER;

    /** Id of the time picker view. */
    public static int ID_TIME_PICKER;

    /** Id of the year picker view. */
    public static int ID_YEAR_PICKER;

    /** Id of the view containing the month and year pickers. */
    public static int ID_MONTH_YEAR_PICKERS_CONTAINER;

    /** Layout of the date/time picker dialog. */
    public static int LAYOUT_DATE_TIME_PICKER_DIALOG;

    /** Layout of the Javascript modal dialog. */
    public static int LAYOUT_JS_MODAL_DIALOG;

    /** Layout of the month picker. */
    public static int LAYOUT_MONTH_PICKER;

    /** Layout of the month picker dialog. */
    public static int LAYOUT_MONTH_PICKER_DIALOG;

    /** Mipmap image used as background for bookmark shortcut icons. */
    public static int MIPMAP_BOOKMARK_SHORTCUT_BACKGROUND;

    /** String for the Share button in the action bar. */
    public static int STRING_ACTION_BAR_SHARE;

    /** String for the Web Search button in the action bar. */
    public static int STRING_ACTION_BAR_WEB_SEARCH;

    /** String for the Content View accessibility contentDescription. */
    public static int STRING_CONTENT_VIEW_CONTENT_DESCRIPTION;

    /** String for the Clear button in the date picker dialog. */
    public static int STRING_DATE_PICKER_DIALOG_CLEAR;

    /** String for the Set button in the date picker dialog. */
    public static int STRING_DATE_PICKER_DIALOG_SET;

    /** String for the title of the date/time picker dialog. */
    public static int STRING_DATE_TIME_PICKER_DIALOG_TITLE;

    /** String for 'Don't reload this page' in Javascript modal dialogs. */
    public static int STRING_JS_MODAL_DIALOG_DONT_RELOAD_THIS_PAGE;

    /** String for 'Leave this page' in Javascript modal dialogs. */
    public static int STRING_JS_MODAL_DIALOG_LEAVE_THIS_PAGE;

    /** String for 'Reload this page' in Javascript modal dialogs. */
    public static int STRING_JS_MODAL_DIALOG_RELOAD_THIS_PAGE;

    /** String for 'Stay on this page' in Javascript modal dialogs. */
    public static int STRING_JS_MODAL_DIALOG_STAY_ON_THIS_PAGE;

    /** String for the title of the month picker dialog. */
    public static int STRING_MONTH_PICKER_DIALOG_TITLE;

    /** String for playback errors in the media player. */
    public static int STRING_MEDIA_PLAYER_MESSAGE_PLAYBACK_ERROR;

    /** String for unknown errors in the media player. */
    public static int STRING_MEDIA_PLAYER_MESSAGE_UNKNOWN_ERROR;

    /** String for the button contents in the media player error dialog. */
    public static int STRING_MEDIA_PLAYER_ERROR_BUTTON;

    /** String for the title of the media player error dialog. */
    public static int STRING_MEDIA_PLAYER_ERROR_TITLE;

    /**
     * Iterates through all the resources ids and verifies they have values other than zero.
     * @return true if all the resources have been registered.
     */
    public static boolean verifyResourceRegistration() {
        Field[] fields = AppResource.class.getDeclaredFields();
        for (Field field : fields) {
            try {
              if (field.getType().equals(int.class) && Modifier.isStatic(field.getModifiers())) {
                  if (field.getInt(null) == 0) return false;
              }
            } catch (IllegalAccessException e) {
            }
        }
        return true;
    }
}
