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
