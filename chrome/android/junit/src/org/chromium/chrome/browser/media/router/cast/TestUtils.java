// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.cast;

import android.support.v7.media.MediaRouter;
import android.support.v7.media.MediaRouter.RouteInfo;

import org.robolectric.internal.ReflectionHelpers;

/**
 * Utility classes and methods for MediaRouterTests.
 */
public class TestUtils {
    /**
     * Creates a mock {@link RouteInfo} to supply where needed in the tests.
     * @param id The id of the route
     * @param name The user friendly name of the route
     * @return The initialized mock RouteInfo instance
     */
    static RouteInfo createMockRouteInfo(String id, String name) {
        Class<?>[] paramClasses = new Class[] {
                MediaRouter.ProviderInfo.class, String.class, String.class};
        Object[] paramValues = new Object[] {null, "", ""};
        RouteInfo routeInfo = ReflectionHelpers.callConstructorReflectively(
                RouteInfo.class,
                ReflectionHelpers.ClassParameter.fromComponentLists(paramClasses, paramValues));
        ReflectionHelpers.setFieldReflectively(routeInfo, "mUniqueId", id);
        ReflectionHelpers.setFieldReflectively(routeInfo, "mName", name);
        return routeInfo;
    }
}
