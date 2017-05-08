// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.process_launcher.ChildProcessCreationParams;

/**
 * A connection that is bound as important, meaning the framework brings it to the foreground
 * process level when the app is.
 */
public class ImportantChildProcessConnection extends BaseChildProcessConnection {
    public static final Factory FACTORY = new BaseChildProcessConnection.Factory() {
        @Override
        public BaseChildProcessConnection create(Context context, DeathCallback deathCallback,
                String serviceClassName, Bundle childProcessCommonParameters,
                ChildProcessCreationParams creationParams) {
            return new ImportantChildProcessConnection(context, deathCallback, serviceClassName,
                    childProcessCommonParameters, creationParams);
        }
    };

    private final ChildServiceConnection mBinding;

    private ImportantChildProcessConnection(Context context, DeathCallback deathCallback,
            String serviceClassName, Bundle childProcessCommonParameters,
            ChildProcessCreationParams creationParams) {
        super(context, deathCallback, serviceClassName, childProcessCommonParameters,
                creationParams);
        int flags = Context.BIND_AUTO_CREATE | Context.BIND_IMPORTANT;
        if (shouldBindAsExportedService()) {
            flags |= Context.BIND_EXTERNAL_SERVICE;
        }
        mBinding = createServiceConnection(flags);
    }

    @Override
    public boolean bind() {
        return mBinding.bind();
    }

    @Override
    public void unbind() {
        mBinding.unbind();
    }
}
