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

    /** Dimension of the radius used in the link preview overlay. */
    public static int DIMENSION_LINK_PREVIEW_OVERLAY_RADIUS;

    /** Drawable resource for the link preview popup overlay. */
    public static int DRAWABLE_LINK_PREVIEW_POPUP_OVERLAY;

    /** Id of the date picker view. */
    public static int ID_DATE_PICKER;

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

    /** Layout of the month picker. */
    public static int LAYOUT_MONTH_PICKER;

    /** Layout of the month picker dialog. */
    public static int LAYOUT_MONTH_PICKER_DIALOG;

    /** String for the Clear button in the date picker dialog. */
    public static int STRING_DATE_PICKER_DIALOG_CLEAR;

    /** String for the Set button in the date picker dialog. */
    public static int STRING_DATE_PICKER_DIALOG_SET;

    /** String for the title of the date/time picker dialog. */
    public static int STRING_DATE_TIME_PICKER_DIALOG_TITLE;

    /** String for the title of the month picker dialog. */
    public static int STRING_MONTH_PICKER_DIALOG_TITLE;

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
