// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.support.test.InstrumentationRegistry;
import android.support.v4.content.ContextCompat;
import android.text.TextUtils;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.InMemorySharedPreferencesContext;

import java.io.File;
import java.util.ArrayList;

/**
 * Holds setUp / tearDown logic common to all instrumentation tests.
 */
class BaseJUnit4TestRule implements TestRule {
    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                // Don't tests if there are prior on-disk shared prefs lying around.
                checkOrDeleteOnDiskSharedPreferences(false);

                InMemorySharedPreferencesContext context =
                        BaseChromiumAndroidJUnitRunner.sInMemorySharedPreferencesContext;
                // Reset Application context in case any tests have replaced it.
                ContextUtils.initApplicationContextForTests(context);
                // Ensure all tests start with empty (InMemory)SharedPreferences.
                context.clearSharedPreferences();

                base.evaluate();

                // Do not use try/finally so that preferences asserts do not mask prior exceptions.
                checkOrDeleteOnDiskSharedPreferences(true);
            }
        };
    }

    private void checkOrDeleteOnDiskSharedPreferences(boolean check) {
        File dataDir = ContextCompat.getDataDir(InstrumentationRegistry.getTargetContext());
        File prefsDir = new File(dataDir, "shared_prefs");
        File[] files = prefsDir.listFiles();
        if (files == null) {
            return;
        }
        ArrayList<File> badFiles = new ArrayList<>();
        for (File f : files) {
            // Multidex support library prefs need to stay or else multidex extraction will occur
            // needlessly.
            // WebView prefs need to stay because webview tests have no (good) way of hooking
            // SharedPreferences for instantiated WebViews.
            if (!f.getName().endsWith("multidex.version.xml")
                    && !f.getName().equals("WebViewChromiumPrefs.xml")) {
                if (check) {
                    badFiles.add(f);
                } else {
                    f.delete();
                }
            }
        }
        if (!badFiles.isEmpty()) {
            throw new AssertionError("Found shared prefs file(s).\n"
                    + "Code should use ContextUtils.getApplicationContext() when accessing "
                    + "SharedPreferences so that tests are hooked to use InMemorySharedPreferences."
                    + " This could also mean needing to override getSharedPreferences() on custom "
                    + " Context subclasses (e.g. ChromeBaseAppCompatActivity does this to make "
                    + "Preferences screens work).\n"
                    + "Files:\n * " + TextUtils.join("\n * ", badFiles));
        }
    }
}
