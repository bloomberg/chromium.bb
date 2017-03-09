// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;
import org.chromium.content_shell_apk.ContentShellTestCommon.TestCommonCallback;

import java.lang.annotation.Annotation;

/**
 * Common functionality for testing the Java Bridge.
 */
public class JavaBridgeTestBase
        extends ContentShellTestBase implements TestCommonCallback<ContentShellActivity> {
    private final JavaBridgeTestCommon mTestCommon = new JavaBridgeTestCommon(this);

    /**
     * Sets up the ContentView. Intended to be called from setUp().
     */
    private void setUpContentView() throws Exception {
        mTestCommon.setUpContentView();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestCommon.setUpContentView();
    }

    public TestCallbackHelperContainer getTestCallBackHelperContainer() {
        return mTestCommon.getTestCallBackHelperContainer();
    }

    protected void executeJavaScript(final String script) throws Throwable {
        mTestCommon.executeJavaScript(script);
    }

    protected void injectObjectAndReload(final Object object, final String name) throws Exception {
        injectObjectAndReload(object, name, null);
    }

    protected void injectObjectAndReload(final Object object, final String name,
            final Class<? extends Annotation> requiredAnnotation) throws Exception {
        injectObjectsAndReload(object, name, null, null, requiredAnnotation);
    }

    protected void injectObjectsAndReload(final Object object1, final String name1,
            final Object object2, final String name2,
            final Class<? extends Annotation> requiredAnnotation) throws Exception {
        mTestCommon.injectObjectsAndReload(object1, name1, object2, name2, requiredAnnotation);
    }

    protected void synchronousPageReload() throws Throwable {
        mTestCommon.synchronousPageReload();
    }
}
