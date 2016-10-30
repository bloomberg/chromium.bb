// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;

import java.lang.reflect.Constructor;

/**
 * Replaceable class providing ability to load Cronet implementation.
 */
final class ImplLoader {
    // The class name of the Cronet Engine Builder implementation.
    private static final String CRONET_ENGINE_BUILDER_IMPL =
            "org.chromium.net.impl.CronetEngineBuilderImpl";

    /**
     * Load implementation of {@link ICronetEngineBuilder}.
     * @param context Android {@link Context} to use for loading.
     * @return {@link ICronetEngineBuilder} implementation.
     */
    static ICronetEngineBuilder load(Context context) {
        try {
            Class<? extends ICronetEngineBuilder> delegateImplClass =
                    Class.forName(CRONET_ENGINE_BUILDER_IMPL)
                            .asSubclass(ICronetEngineBuilder.class);
            Constructor<? extends ICronetEngineBuilder> ctor =
                    delegateImplClass.getConstructor(Context.class);
            return ctor.newInstance(context);
        } catch (Exception e) {
            throw new RuntimeException(
                    "Unable to construct the implementation of the Cronet Engine Builder: "
                            + CRONET_ENGINE_BUILDER_IMPL,
                    e);
        }
    }
}