// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.test.dex_optimizer;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.base.FileUtils;
import org.chromium.webapk.lib.client.DexOptimizer;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class DexOptimizerServiceImpl extends Service {
    private static int sCounter = 0;

    public IBinder onBind(Intent intent) {
        return new IDexOptimizerService.Stub() {
            private static final String DEX_ASSET_NAME = "canary.dex";
            private static final String DEX_DIR = "dex";

            @Override
            public boolean deleteDexDirectory() {
                File dir = getDir(DEX_DIR, Context.MODE_PRIVATE);
                FileUtils.recursivelyDeleteFile(dir);
                return !dir.exists();
            }

            @Override
            public String extractAndOptimizeDex() {
                // DexClassLoader does not generate an optimized dex if a DexClassLoader was
                // previously constructed for the exact same dex path (presumably due to
                // cached state in Android). This restriction does not seem to hold if
                // DexOptimizerService is restarted. Hack around this restriction by extracting the
                // dex from the APK to a different file every time. The hack is okay because
                // deleting a dex file and immediately recreating an identical one is only done in
                // tests.
                String dexName = "canary" + sCounter + ".dex";
                ++sCounter;

                File dexFile = new File(getDir(DEX_DIR, Context.MODE_PRIVATE), dexName);
                if (!extractAsset(DEX_ASSET_NAME, dexFile)) {
                    return null;
                }

                // Make dex file world readable.
                try {
                    dexFile.setReadable(true, false);
                } catch (Exception e) {
                    return null;
                }

                if (!DexOptimizer.optimize(dexFile)) {
                    return null;
                }
                return dexFile.getPath();
            }


            /**
             * Extracts an asset from the app's APK to a file.
             * @param assetName Name of the asset to extract.
             * @param dest File to extract the asset to.
             * @return true on success.
             */
            public boolean extractAsset(String assetName, File dest) {
                try (InputStream inputStream = getAssets().open(assetName);
                    OutputStream outputStream = new FileOutputStream(dest)) {
                    byte[] buffer = new byte[16 * 1024];
                    int c;
                    while ((c = inputStream.read(buffer, 0, buffer.length)) != -1) {
                        outputStream.write(buffer, 0, c);
                    }
                } catch (IOException e) {
                    return false;
                }
                return true;
            }
        };
    }
}
