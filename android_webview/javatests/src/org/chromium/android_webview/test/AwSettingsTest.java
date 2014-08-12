// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.graphics.Point;
import android.net.http.SslError;
import android.os.Build;
import android.os.SystemClock;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.WindowManager;
import android.webkit.JavascriptInterface;
import android.webkit.ValueCallback;
import android.webkit.WebSettings;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import org.apache.http.Header;
import org.apache.http.HttpRequest;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient.ShouldInterceptRequestParams;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.AwSettings.LayoutAlgorithm;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.ImagePageGenerator;
import org.chromium.android_webview.test.util.VideoTestUtil;
import org.chromium.android_webview.test.util.VideoTestWebServer;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.HistoryUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.gfx.DeviceDisplayInfo;

import java.io.File;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * A test suite for AwSettings class. The key objective is to verify that each
 * settings applies either to each individual view or to all views of the
 * application
 */
public class AwSettingsTest extends AwTestBase {
    private static final boolean ENABLED = true;
    private static final boolean DISABLED = false;

    /**
     * A helper class for testing a particular preference from AwSettings.
     * The generic type T is the type of the setting. Usually, to test an
     * effect of the preference, JS code is executed that sets document's title.
     * In this case, requiresJsEnabled constructor argument must be set to true.
     */
    abstract class AwSettingsTestHelper<T> {
        protected final AwContents mAwContents;
        protected final Context mContext;
        protected final TestAwContentsClient mContentViewClient;
        protected final AwSettings mAwSettings;

        AwSettingsTestHelper(AwTestContainerView containerView,
                             TestAwContentsClient contentViewClient,
                             boolean requiresJsEnabled) throws Throwable {
            mAwContents = containerView.getAwContents();
            mContext = containerView.getContext();
            mContentViewClient = contentViewClient;
            mAwSettings = AwSettingsTest.this.getAwSettingsOnUiThread(mAwContents);
            if (requiresJsEnabled) {
                mAwSettings.setJavaScriptEnabled(true);
            }
        }

        void ensureSettingHasAlteredValue() throws Throwable {
            ensureSettingHasValue(getAlteredValue());
        }

        void ensureSettingHasInitialValue() throws Throwable {
            ensureSettingHasValue(getInitialValue());
        }

        void setAlteredSettingValue() throws Throwable {
            setCurrentValue(getAlteredValue());
        }

        void setInitialSettingValue() throws Throwable {
            setCurrentValue(getInitialValue());
        }

        protected abstract T getAlteredValue();

        protected abstract T getInitialValue();

        protected abstract T getCurrentValue();

        protected abstract void setCurrentValue(T value) throws Throwable;

        protected abstract void doEnsureSettingHasValue(T value) throws Throwable;

        protected String getTitleOnUiThread() throws Exception {
            return AwSettingsTest.this.getTitleOnUiThread(mAwContents);
        }

        protected void loadDataSync(String data) throws Throwable {
            AwSettingsTest.this.loadDataSync(
                mAwContents,
                mContentViewClient.getOnPageFinishedHelper(),
                data,
                "text/html",
                false);
        }

        protected void loadUrlSync(String url) throws Throwable {
            AwSettingsTest.this.loadUrlSync(
                mAwContents,
                mContentViewClient.getOnPageFinishedHelper(),
                url);
        }

        protected void loadUrlSyncAndExpectError(String url) throws Throwable {
            AwSettingsTest.this.loadUrlSyncAndExpectError(
                mAwContents,
                mContentViewClient.getOnPageFinishedHelper(),
                mContentViewClient.getOnReceivedErrorHelper(),
                url);
        }

        protected String executeJavaScriptAndWaitForResult(String script) throws Exception {
            return AwSettingsTest.this.executeJavaScriptAndWaitForResult(
                    mAwContents, mContentViewClient, script);
        }

        private void ensureSettingHasValue(T value) throws Throwable {
            assertEquals(value, getCurrentValue());
            doEnsureSettingHasValue(value);
        }
    }

    class AwSettingsJavaScriptTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String JS_ENABLED_STRING = "JS Enabled";
        private static final String JS_DISABLED_STRING = "JS Disabled";

        AwSettingsJavaScriptTestHelper(AwTestContainerView containerView,
                                       TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, false);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getJavaScriptEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setJavaScriptEnabled(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            assertEquals(
                value == ENABLED ? JS_ENABLED_STRING : JS_DISABLED_STRING,
                getTitleOnUiThread());
        }

        private String getData() {
            return "<html><head><title>" + JS_DISABLED_STRING + "</title>"
                    + "</head><body onload=\"document.title='" + JS_ENABLED_STRING
                    + "';\"></body></html>";
        }
    }

    // In contrast to AwSettingsJavaScriptTestHelper, doesn't reload the page when testing
    // JavaScript state.
    class AwSettingsJavaScriptDynamicTestHelper extends AwSettingsJavaScriptTestHelper {
        AwSettingsJavaScriptDynamicTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient);
            // Load the page.
            super.doEnsureSettingHasValue(getInitialValue());
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            String oldTitle = getTitleOnUiThread();
            String newTitle = oldTitle + "_modified";
            executeJavaScriptAndWaitForResult(getScript(newTitle));
            assertEquals(value == ENABLED ? newTitle : oldTitle, getTitleOnUiThread());
        }

        private String getScript(String title) {
            return "document.title='" + title + "';";
        }
    }

    class AwSettingsPluginsTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String PLUGINS_ENABLED_STRING = "Embed";
        private static final String PLUGINS_DISABLED_STRING = "NoEmbed";

        AwSettingsPluginsTestHelper(AwTestContainerView containerView,
                                    TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, true);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getPluginsEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setPluginsEnabled(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            assertEquals(
                value == ENABLED ? PLUGINS_ENABLED_STRING : PLUGINS_DISABLED_STRING,
                getTitleOnUiThread());
        }

        private String getData() {
            return "<html><body onload=\"document.title = document.body.innerText;\">"
                    + "<noembed>No</noembed><span>Embed</span></body></html>";
        }
    }

    class AwSettingsStandardFontFamilyTestHelper extends AwSettingsTestHelper<String> {
        AwSettingsStandardFontFamilyTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, true);
        }

        @Override
        protected String getAlteredValue() {
            return "cursive";
        }

        @Override
        protected String getInitialValue() {
            return "sans-serif";
        }

        @Override
        protected String getCurrentValue() {
            return mAwSettings.getStandardFontFamily();
        }

        @Override
        protected void setCurrentValue(String value) {
            mAwSettings.setStandardFontFamily(value);
        }

        @Override
        protected void doEnsureSettingHasValue(String value) throws Throwable {
            loadDataSync(getData());
            assertEquals(value, getTitleOnUiThread());
        }

        private String getData() {
            return "<html><body onload=\"document.title = " +
                    "getComputedStyle(document.body).getPropertyValue('font-family');\">"
                    + "</body></html>";
        }
    }

    class AwSettingsDefaultFontSizeTestHelper extends AwSettingsTestHelper<Integer> {
        AwSettingsDefaultFontSizeTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, true);
        }

        @Override
        protected Integer getAlteredValue() {
            return 42;
        }

        @Override
        protected Integer getInitialValue() {
            return 16;
        }

        @Override
        protected Integer getCurrentValue() {
            return mAwSettings.getDefaultFontSize();
        }

        @Override
        protected void setCurrentValue(Integer value) {
            mAwSettings.setDefaultFontSize(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Integer value) throws Throwable {
            loadDataSync(getData());
            assertEquals(value.toString() + "px", getTitleOnUiThread());
        }

        private String getData() {
            return "<html><body onload=\"document.title = " +
                    "getComputedStyle(document.body).getPropertyValue('font-size');\">"
                    + "</body></html>";
        }
    }

    class AwSettingsLoadImagesAutomaticallyTestHelper extends AwSettingsTestHelper<Boolean> {
        private ImagePageGenerator mGenerator;

        AwSettingsLoadImagesAutomaticallyTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                ImagePageGenerator generator) throws Throwable {
            super(containerView, contentViewClient, true);
            mGenerator = generator;
        }

        @Override
        protected Boolean getAlteredValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getLoadsImagesAutomatically();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setLoadsImagesAutomatically(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(mGenerator.getPageSource());
            assertEquals(value == ENABLED ?
                         ImagePageGenerator.IMAGE_LOADED_STRING :
                         ImagePageGenerator.IMAGE_NOT_LOADED_STRING,
                         getTitleOnUiThread());
        }
    }


    class AwSettingsImagesEnabledHelper extends AwSettingsTestHelper<Boolean> {

        AwSettingsImagesEnabledHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                TestWebServer webServer,
                ImagePageGenerator generator) throws Throwable {
            super(containerView, contentViewClient, true);
            mWebServer = webServer;
            mGenerator = generator;
        }

        @Override
        protected Boolean getAlteredValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getImagesEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setImagesEnabled(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            final String httpImageUrl = mGenerator.getPageUrl(mWebServer);
            AwSettingsTest.this.loadUrlSync(
                    mAwContents,
                    mContentViewClient.getOnPageFinishedHelper(),
                    httpImageUrl);
            assertEquals(value == ENABLED ?
                         ImagePageGenerator.IMAGE_LOADED_STRING :
                         ImagePageGenerator.IMAGE_NOT_LOADED_STRING,
                         getTitleOnUiThread());
        }

        private TestWebServer mWebServer;
        private ImagePageGenerator mGenerator;
    }

    class AwSettingsDefaultTextEncodingTestHelper extends AwSettingsTestHelper<String> {
        AwSettingsDefaultTextEncodingTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, true);
        }

        @Override
        protected String getAlteredValue() {
            return "utf-8";
        }

        @Override
        protected String getInitialValue() {
            return "Latin-1";
        }

        @Override
        protected String getCurrentValue() {
            return mAwSettings.getDefaultTextEncodingName();
        }

        @Override
        protected void setCurrentValue(String value) {
            mAwSettings.setDefaultTextEncodingName(value);
        }

        @Override
        protected void doEnsureSettingHasValue(String value) throws Throwable {
            loadDataSync(getData());
            assertEquals(value, getTitleOnUiThread());
        }

        private String getData() {
            return "<html><body onload='document.title=document.defaultCharset'></body></html>";
        }
    }

    class AwSettingsUserAgentStringTestHelper extends AwSettingsTestHelper<String> {
        private final String mDefaultUa;
        private static final String DEFAULT_UA = "";
        private static final String CUSTOM_UA = "ChromeViewTest";

        AwSettingsUserAgentStringTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, true);
            mDefaultUa = mAwSettings.getUserAgentString();
        }

        @Override
        protected String getAlteredValue() {
            return CUSTOM_UA;
        }

        @Override
        protected String getInitialValue() {
            return DEFAULT_UA;
        }

        @Override
        protected String getCurrentValue() {
            // The test framework expects that getXXX() == Z after setXXX(Z).
            // But setUserAgentString("" / null) resets the UA string to default,
            // and getUserAgentString returns the default UA string afterwards.
            // To align with the framework, we return an empty string instead of
            // the default UA.
            String currentUa = mAwSettings.getUserAgentString();
            return mDefaultUa.equals(currentUa) ? DEFAULT_UA : currentUa;
        }

        @Override
        protected void setCurrentValue(String value) {
            mAwSettings.setUserAgentString(value);
        }

        @Override
        protected void doEnsureSettingHasValue(String value) throws Throwable {
            loadDataSync(getData());
            assertEquals(
                DEFAULT_UA.equals(value) ? mDefaultUa : value,
                getTitleOnUiThread());
        }

        private String getData() {
            return "<html><body onload='document.title=navigator.userAgent'></body></html>";
        }
    }

    class AwSettingsDomStorageEnabledTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String TEST_FILE = "webview/localStorage.html";
        private static final String NO_LOCAL_STORAGE = "No localStorage";
        private static final String HAS_LOCAL_STORAGE = "Has localStorage";

        AwSettingsDomStorageEnabledTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, true);
            AwSettingsTest.assertFileIsReadable(UrlUtils.getTestFilePath(TEST_FILE));
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getDomStorageEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setDomStorageEnabled(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            // It is not permitted to access localStorage from data URLs in WebKit,
            // that is why a standalone page must be used.
            loadUrlSync(UrlUtils.getTestFileUrl(TEST_FILE));
            assertEquals(
                value == ENABLED ? HAS_LOCAL_STORAGE : NO_LOCAL_STORAGE,
                getTitleOnUiThread());
        }
    }

    class AwSettingsDatabaseTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String TEST_FILE = "webview/database_access.html";
        private static final String NO_DATABASE = "No database";
        private static final String HAS_DATABASE = "Has database";

        AwSettingsDatabaseTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, true);
            AwSettingsTest.assertFileIsReadable(UrlUtils.getTestFilePath(TEST_FILE));
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getDatabaseEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setDatabaseEnabled(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            // It seems accessing the database through a data scheme is not
            // supported, and fails with a DOM exception (likely a cross-domain
            // violation).
            loadUrlSync(UrlUtils.getTestFileUrl(TEST_FILE));
            assertEquals(
                value == ENABLED ? HAS_DATABASE : NO_DATABASE,
                getTitleOnUiThread());
        }
    }

    class AwSettingsUniversalAccessFromFilesTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String TEST_CONTAINER_FILE = "webview/iframe_access.html";
        private static final String TEST_FILE = "webview/hello_world.html";
        private static final String ACCESS_DENIED_TITLE = "Exception";

        AwSettingsUniversalAccessFromFilesTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, true);
            AwSettingsTest.assertFileIsReadable(UrlUtils.getTestFilePath(TEST_CONTAINER_FILE));
            AwSettingsTest.assertFileIsReadable(UrlUtils.getTestFilePath(TEST_FILE));
            mIframeContainerUrl = UrlUtils.getTestFileUrl(TEST_CONTAINER_FILE);
            mIframeUrl = UrlUtils.getTestFileUrl(TEST_FILE);
            // The value of the setting depends on the SDK version.
            mAwSettings.setAllowUniversalAccessFromFileURLs(false);
            // If universal access is true, the value of file access doesn't
            // matter. While if universal access is false, having file access
            // enabled will allow file loading.
            mAwSettings.setAllowFileAccessFromFileURLs(false);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getAllowUniversalAccessFromFileURLs();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setAllowUniversalAccessFromFileURLs(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadUrlSync(mIframeContainerUrl);
            assertEquals(
                value == ENABLED ? mIframeUrl : ACCESS_DENIED_TITLE,
                getTitleOnUiThread());
        }

        private final String mIframeContainerUrl;
        private final String mIframeUrl;
    }

    class AwSettingsFileAccessFromFilesIframeTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String TEST_CONTAINER_FILE = "webview/iframe_access.html";
        private static final String TEST_FILE = "webview/hello_world.html";
        private static final String ACCESS_DENIED_TITLE = "Exception";

        AwSettingsFileAccessFromFilesIframeTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, true);
            AwSettingsTest.assertFileIsReadable(UrlUtils.getTestFilePath(TEST_CONTAINER_FILE));
            AwSettingsTest.assertFileIsReadable(UrlUtils.getTestFilePath(TEST_FILE));
            mIframeContainerUrl = UrlUtils.getTestFileUrl(TEST_CONTAINER_FILE);
            mIframeUrl = UrlUtils.getTestFileUrl(TEST_FILE);
            mAwSettings.setAllowUniversalAccessFromFileURLs(false);
            // The value of the setting depends on the SDK version.
            mAwSettings.setAllowFileAccessFromFileURLs(false);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getAllowFileAccessFromFileURLs();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setAllowFileAccessFromFileURLs(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadUrlSync(mIframeContainerUrl);
            assertEquals(
                value == ENABLED ? mIframeUrl : ACCESS_DENIED_TITLE,
                getTitleOnUiThread());
        }

        private final String mIframeContainerUrl;
        private final String mIframeUrl;
    }

    class AwSettingsFileAccessFromFilesXhrTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String TEST_FILE = "webview/xhr_access.html";
        private static final String ACCESS_GRANTED_TITLE = "Hello, World!";
        private static final String ACCESS_DENIED_TITLE = "Exception";

        AwSettingsFileAccessFromFilesXhrTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, true);
            assertFileIsReadable(UrlUtils.getTestFilePath(TEST_FILE));
            mXhrContainerUrl = UrlUtils.getTestFileUrl(TEST_FILE);
            mAwSettings.setAllowUniversalAccessFromFileURLs(false);
            // The value of the setting depends on the SDK version.
            mAwSettings.setAllowFileAccessFromFileURLs(false);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getAllowFileAccessFromFileURLs();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setAllowFileAccessFromFileURLs(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadUrlSync(mXhrContainerUrl);
            assertEquals(
                value == ENABLED ? ACCESS_GRANTED_TITLE : ACCESS_DENIED_TITLE,
                getTitleOnUiThread());
        }

        private final String mXhrContainerUrl;
    }

    class AwSettingsFileUrlAccessTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String TEST_FILE = "webview/hello_world.html";
        private static final String ACCESS_GRANTED_TITLE = "Hello, World!";

        AwSettingsFileUrlAccessTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                int startIndex) throws Throwable {
            super(containerView, contentViewClient, true);
            mIndex = startIndex;
            AwSettingsTest.assertFileIsReadable(UrlUtils.getTestFilePath(TEST_FILE));
        }

        @Override
        protected Boolean getAlteredValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getAllowFileAccess();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setAllowFileAccess(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            // Use query parameters to avoid hitting a cached page.
            String fileUrl = UrlUtils.getTestFileUrl(TEST_FILE + "?id=" + mIndex);
            mIndex += 2;
            if (value == ENABLED) {
                loadUrlSync(fileUrl);
                assertEquals(ACCESS_GRANTED_TITLE, getTitleOnUiThread());
            } else {
                loadUrlSyncAndExpectError(fileUrl);
            }
        }

        private int mIndex;
    }

    class AwSettingsContentUrlAccessTestHelper extends AwSettingsTestHelper<Boolean> {

        AwSettingsContentUrlAccessTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                int index) throws Throwable {
            super(containerView, contentViewClient, true);
            mTarget = "content_access_" + index;
        }

        @Override
        protected Boolean getAlteredValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getAllowContentAccess();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setAllowContentAccess(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            AwSettingsTest.this.resetResourceRequestCountInContentProvider(mTarget);
            if (value == ENABLED) {
                loadUrlSync(AwSettingsTest.this.createContentUrl(mTarget));
                String title = getTitleOnUiThread();
                assertTrue(title != null);
                assertTrue("[" + mTarget + "] Actual title: \"" + title + "\"",
                        title.contains(mTarget));
                AwSettingsTest.this.ensureResourceRequestCountInContentProvider(mTarget, 1);
            } else {
                loadUrlSyncAndExpectError(AwSettingsTest.this.createContentUrl(mTarget));
                AwSettingsTest.this.ensureResourceRequestCountInContentProvider(mTarget, 0);
            }
        }

        private final String mTarget;
    }

    class AwSettingsContentUrlAccessFromFileTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String TARGET = "content_from_file";

        AwSettingsContentUrlAccessFromFileTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                int index) throws Throwable {
            super(containerView, contentViewClient, true);
            mIndex = index;
            mTempDir = getInstrumentation().getTargetContext().getCacheDir().getPath();
        }

        @Override
        protected Boolean getAlteredValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getAllowContentAccess();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setAllowContentAccess(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            AwSettingsTest.this.resetResourceRequestCountInContentProvider(TARGET);
            final String fileName = mTempDir + "/" + TARGET + ".html";
            try {
                TestFileUtil.createNewHtmlFile(fileName,
                        TARGET,
                        "<img src=\"" +
                        // Adding a query avoids hitting a cached image, and also verifies
                        // that content URL query parameters are ignored when accessing
                        // a content provider.
                        AwSettingsTest.this.createContentUrl(TARGET + "?id=" + mIndex) + "\">");
                mIndex += 2;
                loadUrlSync("file://" + fileName);
                if (value == ENABLED) {
                    AwSettingsTest.this.ensureResourceRequestCountInContentProvider(TARGET, 1);
                } else {
                    AwSettingsTest.this.ensureResourceRequestCountInContentProvider(TARGET, 0);
                }
            } finally {
                TestFileUtil.deleteFile(fileName);
            }
        }

        private int mIndex;
        private String mTempDir;
    }

    // This class provides helper methods for testing of settings related to
    // the text autosizing feature.
    abstract class AwSettingsTextAutosizingTestHelper<T> extends AwSettingsTestHelper<T> {
        protected static final float PARAGRAPH_FONT_SIZE = 14.0f;

        AwSettingsTextAutosizingTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, true);
            mNeedToWaitForFontSizeChange = false;
            loadDataSync(getData());
        }

        @Override
        protected void setCurrentValue(T value) throws Throwable {
            mNeedToWaitForFontSizeChange = false;
            if (value != getCurrentValue()) {
                mOldFontSize = getActualFontSize();
                mNeedToWaitForFontSizeChange = true;
            }
        }

        protected float getActualFontSize() throws Throwable {
            if (!mNeedToWaitForFontSizeChange) {
                executeJavaScriptAndWaitForResult("setTitleToActualFontSize()");
            } else {
                final float oldFontSize = mOldFontSize;
                poll(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        executeJavaScriptAndWaitForResult("setTitleToActualFontSize()");
                        float newFontSize = Float.parseFloat(getTitleOnUiThread());
                        return newFontSize != oldFontSize;
                    }
                });
                mNeedToWaitForFontSizeChange = false;
            }
            return Float.parseFloat(getTitleOnUiThread());
        }

        protected String getData() {
            DeviceDisplayInfo deviceInfo = DeviceDisplayInfo.create(mContext);
            int displayWidth = (int) (deviceInfo.getDisplayWidth() / deviceInfo.getDIPScale());
            int layoutWidth = (int) (displayWidth * 2.5f); // Use 2.5 as autosizing layout tests do.
            StringBuilder sb = new StringBuilder();
            sb.append("<html>" +
                    "<head>" +
                    "<meta name=\"viewport\" content=\"width=" + layoutWidth + "\">" +
                    "<style>" +
                    "body { width: " + layoutWidth + "px; margin: 0; overflow-y: hidden; }" +
                    "</style>" +
                    "<script>" +
                    "function setTitleToActualFontSize() {" +
                    // parseFloat is used to trim out the "px" suffix.
                    "  document.title = parseFloat(getComputedStyle(" +
                    "    document.getElementById('par')).getPropertyValue('font-size'));" +
                    "}</script></head>" +
                    "<body>" +
                    "<p id=\"par\" style=\"font-size:");
            sb.append(PARAGRAPH_FONT_SIZE);
            sb.append("px;\">");
            // Make the paragraph wide enough for being processed by the font autosizer.
            for (int i = 0; i < 500; i++) {
                sb.append("Hello, World! ");
            }
            sb.append("</p></body></html>");
            return sb.toString();
        }

        private boolean mNeedToWaitForFontSizeChange;
        private float mOldFontSize;
    }

    class AwSettingsLayoutAlgorithmTestHelper extends
                                              AwSettingsTextAutosizingTestHelper<LayoutAlgorithm> {

        AwSettingsLayoutAlgorithmTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient);
            // Font autosizing doesn't step in for narrow layout widths.
            mAwSettings.setUseWideViewPort(true);
        }

        @Override
        protected LayoutAlgorithm getAlteredValue() {
            return LayoutAlgorithm.TEXT_AUTOSIZING;
        }

        @Override
        protected LayoutAlgorithm getInitialValue() {
            return LayoutAlgorithm.NARROW_COLUMNS;
        }

        @Override
        protected LayoutAlgorithm getCurrentValue() {
            return mAwSettings.getLayoutAlgorithm();
        }

        @Override
        protected void setCurrentValue(LayoutAlgorithm value) throws Throwable {
            super.setCurrentValue(value);
            mAwSettings.setLayoutAlgorithm(value);
        }

        @Override
        protected void doEnsureSettingHasValue(LayoutAlgorithm value) throws Throwable {
            final float actualFontSize = getActualFontSize();
            if (value == LayoutAlgorithm.TEXT_AUTOSIZING) {
                assertFalse("Actual font size: " + actualFontSize,
                        actualFontSize == PARAGRAPH_FONT_SIZE);
            } else {
                assertTrue("Actual font size: " + actualFontSize,
                        actualFontSize == PARAGRAPH_FONT_SIZE);
            }
        }
    }

    class AwSettingsTextZoomTestHelper extends AwSettingsTextAutosizingTestHelper<Integer> {
        private static final int INITIAL_TEXT_ZOOM = 100;
        private final float mInitialActualFontSize;

        AwSettingsTextZoomTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient);
            mInitialActualFontSize = getActualFontSize();
        }

        @Override
        protected Integer getAlteredValue() {
            return INITIAL_TEXT_ZOOM * 2;
        }

        @Override
        protected Integer getInitialValue() {
            return INITIAL_TEXT_ZOOM;
        }

        @Override
        protected Integer getCurrentValue() {
            return mAwSettings.getTextZoom();
        }

        @Override
        protected void setCurrentValue(Integer value) throws Throwable {
            super.setCurrentValue(value);
            mAwSettings.setTextZoom(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Integer value) throws Throwable {
            final float actualFontSize = getActualFontSize();
            // Ensure that actual vs. initial font size ratio is similar to actual vs. initial
            // text zoom values ratio.
            final float ratiosDelta = Math.abs(
                (actualFontSize / mInitialActualFontSize) -
                (value / (float) INITIAL_TEXT_ZOOM));
            assertTrue(
                "|(" + actualFontSize + " / " + mInitialActualFontSize + ") - (" +
                value + " / " + INITIAL_TEXT_ZOOM + ")| = " + ratiosDelta,
                ratiosDelta <= 0.2f);
        }
    }

    class AwSettingsTextZoomAutosizingTestHelper
            extends AwSettingsTextAutosizingTestHelper<Integer> {
        private static final int INITIAL_TEXT_ZOOM = 100;
        private final float mInitialActualFontSize;

        AwSettingsTextZoomAutosizingTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient);
            mAwSettings.setLayoutAlgorithm(LayoutAlgorithm.TEXT_AUTOSIZING);
            // The initial font size can be adjusted by font autosizer depending on the page's
            // viewport width.
            mInitialActualFontSize = getActualFontSize();
        }

        @Override
        protected Integer getAlteredValue() {
            return INITIAL_TEXT_ZOOM * 2;
        }

        @Override
        protected Integer getInitialValue() {
            return INITIAL_TEXT_ZOOM;
        }

        @Override
        protected Integer getCurrentValue() {
            return mAwSettings.getTextZoom();
        }

        @Override
        protected void setCurrentValue(Integer value) throws Throwable {
            super.setCurrentValue(value);
            mAwSettings.setTextZoom(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Integer value) throws Throwable {
            final float actualFontSize = getActualFontSize();
            // Ensure that actual vs. initial font size ratio is similar to actual vs. initial
            // text zoom values ratio.
            final float ratiosDelta = Math.abs(
                (actualFontSize / mInitialActualFontSize) -
                (value / (float) INITIAL_TEXT_ZOOM));
            assertTrue(
                "|(" + actualFontSize + " / " + mInitialActualFontSize + ") - (" +
                value + " / " + INITIAL_TEXT_ZOOM + ")| = " + ratiosDelta,
                ratiosDelta <= 0.2f);
        }
    }

    class AwSettingsJavaScriptPopupsTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String POPUP_ENABLED = "Popup enabled";
        private static final String POPUP_BLOCKED = "Popup blocked";

        AwSettingsJavaScriptPopupsTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, true);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getJavaScriptCanOpenWindowsAutomatically();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setJavaScriptCanOpenWindowsAutomatically(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            final boolean expectPopupEnabled = value;
            poll(new Callable<Boolean>() {
                @Override
                public Boolean call() throws Exception {
                    String title = getTitleOnUiThread();
                    return expectPopupEnabled ? POPUP_ENABLED.equals(title) :
                            POPUP_BLOCKED.equals(title);
                }
            });
            assertEquals(value ? POPUP_ENABLED : POPUP_BLOCKED, getTitleOnUiThread());
        }

        private String getData() {
            return "<html><head>" +
                    "<script>" +
                    "    function tryOpenWindow() {" +
                    "        var newWindow = window.open(" +
                    "           'data:text/html;charset=utf-8," +
                    "           <html><head><title>" + POPUP_ENABLED + "</title></head></html>');" +
                    "        if (!newWindow) document.title = '" + POPUP_BLOCKED + "';" +
                    "    }" +
                    "</script></head>" +
                    "<body onload='tryOpenWindow()'></body></html>";
        }
    }

    class AwSettingsCacheModeTestHelper extends AwSettingsTestHelper<Integer> {

        AwSettingsCacheModeTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                int index,
                TestWebServer webServer) throws Throwable {
            super(containerView, contentViewClient, true);
            mIndex = index;
            mWebServer = webServer;
        }

        @Override
        protected Integer getAlteredValue() {
            // We use the value that results in a behaviour completely opposite to default.
            return WebSettings.LOAD_CACHE_ONLY;
        }

        @Override
        protected Integer getInitialValue() {
            return WebSettings.LOAD_DEFAULT;
        }

        @Override
        protected Integer getCurrentValue() {
            return mAwSettings.getCacheMode();
        }

        @Override
        protected void setCurrentValue(Integer value) {
            mAwSettings.setCacheMode(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Integer value) throws Throwable {
            final String htmlPath = "/cache_mode_" + mIndex + ".html";
            mIndex += 2;
            final String url = mWebServer.setResponse(htmlPath, "response", null);
            assertEquals(0, mWebServer.getRequestCount(htmlPath));
            if (value == WebSettings.LOAD_DEFAULT) {
                loadUrlSync(url);
                assertEquals(1, mWebServer.getRequestCount(htmlPath));
            } else {
                loadUrlSyncAndExpectError(url);
                assertEquals(0, mWebServer.getRequestCount(htmlPath));
            }
        }

        private int mIndex;
        private TestWebServer mWebServer;
    }

    // To verify whether UseWideViewport works, we check, if the page width specified
    // in the "meta viewport" tag is applied. When UseWideViewport is turned off, the
    // "viewport" tag is ignored, and the layout width is set to device width in DIP pixels.
    // We specify a very high width value to make sure that it doesn't intersect with
    // device screen widths (in DIP pixels).
    class AwSettingsUseWideViewportTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String VIEWPORT_TAG_LAYOUT_WIDTH = "3000";

        AwSettingsUseWideViewportTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, true);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getUseWideViewPort();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setUseWideViewPort(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            final String bodyWidth = getTitleOnUiThread();
            if (value) {
                assertTrue(bodyWidth, VIEWPORT_TAG_LAYOUT_WIDTH.equals(bodyWidth));
            } else {
                assertFalse(bodyWidth, VIEWPORT_TAG_LAYOUT_WIDTH.equals(bodyWidth));
            }
        }

        private String getData() {
            return "<html><head>" +
                    "<meta name='viewport' content='width=" + VIEWPORT_TAG_LAYOUT_WIDTH + "' />" +
                    "</head>" +
                    "<body onload='document.title=document.body.clientWidth'></body></html>";
        }
    }

    class AwSettingsLoadWithOverviewModeTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final float DEFAULT_PAGE_SCALE = 1.0f;

        AwSettingsLoadWithOverviewModeTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                boolean withViewPortTag) throws Throwable {
            super(containerView, contentViewClient, true);
            mWithViewPortTag = withViewPortTag;
            mAwSettings.setUseWideViewPort(true);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getLoadWithOverviewMode();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mExpectScaleChange = mAwSettings.getLoadWithOverviewMode() != value;
            if (mExpectScaleChange) {
                mOnScaleChangedCallCount =
                        mContentViewClient.getOnScaleChangedHelper().getCallCount();
            }
            mAwSettings.setLoadWithOverviewMode(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            if (mExpectScaleChange) {
                mContentViewClient.getOnScaleChangedHelper().
                        waitForCallback(mOnScaleChangedCallCount);
                mExpectScaleChange = false;
            }
            float currentScale = AwSettingsTest.this.getScaleOnUiThread(mAwContents);
            if (value) {
                assertTrue("Expected: " + currentScale + " < " + DEFAULT_PAGE_SCALE,
                        currentScale < DEFAULT_PAGE_SCALE);
            } else {
                assertEquals(DEFAULT_PAGE_SCALE, currentScale);
            }
        }

        private String getData() {
            return "<html><head>" +
                    (mWithViewPortTag ? "<meta name='viewport' content='width=3000' />" : "") +
                    "</head>" +
                    "<body></body></html>";
        }

        private final boolean mWithViewPortTag;
        private boolean mExpectScaleChange;
        private int mOnScaleChangedCallCount;
    }

    class AwSettingsForceZeroLayoutHeightTestHelper extends AwSettingsTestHelper<Boolean> {

        AwSettingsForceZeroLayoutHeightTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                boolean withViewPortTag) throws Throwable {
            super(containerView, contentViewClient, true);
            mWithViewPortTag = withViewPortTag;
            mAwSettings.setUseWideViewPort(true);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getForceZeroLayoutHeight();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setForceZeroLayoutHeight(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            int height = Integer.parseInt(getTitleOnUiThread());
            if (value) {
                assertEquals(0, height);
            } else {
                assertTrue("Div should be at least 50px high, was: " + height, height >= 50);
            }
        }

        private String getData() {
            return "<html><head>" +
                    (mWithViewPortTag ? "<meta name='viewport' content='height=3000' />" : "") +
                    "  <script type='text/javascript'> " +
                    "    window.addEventListener('load', function(event) { " +
                    "       document.title = document.getElementById('testDiv').clientHeight; " +
                    "    }); " +
                    "  </script> " +
                    "</head>" +
                    "<body> " +
                    "  <div style='height:50px;'>test</div> " +
                    "  <div id='testDiv' style='height:100%;'></div> " +
                    "</body></html>";
        }

        private final boolean mWithViewPortTag;
    }

    class AwSettingsZeroLayoutHeightDisablesViewportQuirkTestHelper extends
            AwSettingsTestHelper<Boolean> {

        AwSettingsZeroLayoutHeightDisablesViewportQuirkTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(containerView, contentViewClient, true);
            mAwSettings.setUseWideViewPort(true);
            mAwSettings.setForceZeroLayoutHeight(true);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getZeroLayoutHeightDisablesViewportQuirk();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setZeroLayoutHeightDisablesViewportQuirk(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            DeviceDisplayInfo deviceInfo = DeviceDisplayInfo.create(mContext);
            int displayWidth = (int) (deviceInfo.getDisplayWidth() / deviceInfo.getDIPScale());

            loadDataSync(getData());
            int width = Integer.parseInt(getTitleOnUiThread());
            if (value) {
                assertEquals(displayWidth, width);
            } else {
                assertEquals(3000, width);
            }
        }

        private String getData() {
            return "<html><head>" +
                    "<meta name='viewport' content='width=3000' />" +
                    "  <script type='text/javascript'> " +
                    "    window.addEventListener('load', function(event) { " +
                    "       document.title = document.documentElement.clientWidth; " +
                    "    }); " +
                    "  </script> " +
                    "</head>" +
                    "<body> " +
                    "  <div style='height:50px;'>test</div> " +
                    "  <div id='testDiv' style='height:100%;'></div> " +
                    "</body></html>";
        }
    }

    // The test verifies that JavaScript is disabled upon WebView
    // creation without accessing AwSettings. If the test passes,
    // it means that WebView-specific web preferences configuration
    // is applied on WebView creation. JS state is used, because it is
    // enabled by default in Chrome, but must be disabled by default
    // in WebView.
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testJavaScriptDisabledByDefault() throws Throwable {
        final String JS_ENABLED_STRING = "JS has run";
        final String JS_DISABLED_STRING = "JS has not run";
        final String TEST_PAGE_HTML =
                "<html><head><title>" + JS_DISABLED_STRING + "</title>"
                + "</head><body onload=\"document.title='" + JS_ENABLED_STRING
                + "';\"></body></html>";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        loadDataSync(
            awContents,
            contentClient.getOnPageFinishedHelper(),
            TEST_PAGE_HTML,
            "text/html",
            false);
        assertEquals(JS_DISABLED_STRING, getTitleOnUiThread(awContents));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testJavaScriptEnabledWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsJavaScriptTestHelper(views.getContainer0(), views.getClient0()),
            new AwSettingsJavaScriptTestHelper(views.getContainer1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testJavaScriptEnabledDynamicWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsJavaScriptDynamicTestHelper(views.getContainer0(), views.getClient0()),
            new AwSettingsJavaScriptDynamicTestHelper(views.getContainer1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testPluginsEnabledWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsPluginsTestHelper(views.getContainer0(), views.getClient0()),
            new AwSettingsPluginsTestHelper(views.getContainer1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testStandardFontFamilyWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsStandardFontFamilyTestHelper(views.getContainer0(), views.getClient0()),
            new AwSettingsStandardFontFamilyTestHelper(views.getContainer1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDefaultFontSizeWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsDefaultFontSizeTestHelper(views.getContainer0(), views.getClient0()),
            new AwSettingsDefaultFontSizeTestHelper(views.getContainer1(), views.getClient1()));
    }

    // The test verifies that after changing the LoadsImagesAutomatically
    // setting value from false to true previously skipped images are
    // automatically loaded.
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testLoadsImagesAutomaticallyNoPageReload() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = getAwSettingsOnUiThread(awContents);
        settings.setJavaScriptEnabled(true);
        ImagePageGenerator generator = new ImagePageGenerator(0, false);
        settings.setLoadsImagesAutomatically(false);
        loadDataSync(awContents,
                     contentClient.getOnPageFinishedHelper(),
                     generator.getPageSource(),
                     "text/html", false);
        assertEquals(ImagePageGenerator.IMAGE_NOT_LOADED_STRING,
                getTitleOnUiThread(awContents));
        settings.setLoadsImagesAutomatically(true);
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return !ImagePageGenerator.IMAGE_NOT_LOADED_STRING.equals(
                        getTitleOnUiThread(awContents));
            }
        });
        assertEquals(ImagePageGenerator.IMAGE_LOADED_STRING, getTitleOnUiThread(awContents));
    }


    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testLoadsImagesAutomaticallyWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsLoadImagesAutomaticallyTestHelper(
                views.getContainer0(), views.getClient0(), new ImagePageGenerator(0, true)),
            new AwSettingsLoadImagesAutomaticallyTestHelper(
                views.getContainer1(), views.getClient1(), new ImagePageGenerator(1, true)));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDefaultTextEncodingWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsDefaultTextEncodingTestHelper(views.getContainer0(), views.getClient0()),
            new AwSettingsDefaultTextEncodingTestHelper(views.getContainer1(), views.getClient1()));
    }

    // The test verifies that the default user agent string follows the format
    // defined in Android CTS tests:
    //
    // Mozilla/5.0 (Linux;[ U;] Android <version>;[ <language>-<country>;]
    // [<devicemodel>;] Build/<buildID>) AppleWebKit/<major>.<minor> (KHTML, like Gecko)
    // Version/<major>.<minor>[ Mobile] Safari/<major>.<minor>
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUserAgentStringDefault() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = getAwSettingsOnUiThread(awContents);
        final String actualUserAgentString = settings.getUserAgentString();
        assertEquals(actualUserAgentString, AwSettings.getDefaultUserAgent());
        final String patternString =
                "Mozilla/5\\.0 \\(Linux;( U;)? Android ([^;]+);( (\\w+)-(\\w+);)?" +
                "\\s?(.*)\\sBuild/(.+)\\) AppleWebKit/(\\d+)\\.(\\d+) \\(KHTML, like Gecko\\) " +
                "Version/\\d+\\.\\d Chrome/\\d+\\.\\d+\\.\\d+\\.\\d+" +
                "( Mobile)? Safari/(\\d+)\\.(\\d+)";
        final Pattern userAgentExpr = Pattern.compile(patternString);
        Matcher patternMatcher = userAgentExpr.matcher(actualUserAgentString);
        assertTrue(String.format("User agent string did not match expected pattern. %nExpected " +
                        "pattern:%n%s%nActual:%n%s", patternString, actualUserAgentString),
                        patternMatcher.find());
        // No country-language code token.
        assertEquals(null, patternMatcher.group(3));
        if ("REL".equals(Build.VERSION.CODENAME)) {
            // Model is only added in release builds
            assertEquals(Build.MODEL, patternMatcher.group(6));
            // Release version is valid only in release builds
            assertEquals(Build.VERSION.RELEASE, patternMatcher.group(2));
        }
        assertEquals(Build.ID, patternMatcher.group(7));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUserAgentStringOverride() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = getAwSettingsOnUiThread(awContents);
        final String defaultUserAgentString = settings.getUserAgentString();

        // Check that an attempt to reset the default UA string has no effect.
        settings.setUserAgentString(null);
        assertEquals(defaultUserAgentString, settings.getUserAgentString());
        settings.setUserAgentString("");
        assertEquals(defaultUserAgentString, settings.getUserAgentString());

        // Check that we can also set the default value.
        settings.setUserAgentString(defaultUserAgentString);
        assertEquals(defaultUserAgentString, settings.getUserAgentString());

        // Set a custom UA string, verify that it can be reset back to default.
        final String customUserAgentString = "AwSettingsTest";
        settings.setUserAgentString(customUserAgentString);
        assertEquals(customUserAgentString, settings.getUserAgentString());
        settings.setUserAgentString(null);
        assertEquals(defaultUserAgentString, settings.getUserAgentString());
    }

    // Verify that the current UA override setting has a priority over UA
    // overrides in navigation history entries.
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUserAgentStringOverrideForHistory() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final ContentViewCore contentView = testContainerView.getContentViewCore();
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();
        AwSettings settings = getAwSettingsOnUiThread(awContents);
        settings.setJavaScriptEnabled(true);
        final String defaultUserAgentString = settings.getUserAgentString();
        final String customUserAgentString = "AwSettingsTest";
        // We are using different page titles to make sure that we are really
        // going back and forward between them.
        final String pageTemplate =
                "<html><head><title>%s</title></head>" +
                "<body onload='document.title+=navigator.userAgent'></body>" +
                "</html>";
        final String page1Title = "Page1";
        final String page2Title = "Page2";
        final String page1 = String.format(pageTemplate, page1Title);
        final String page2 = String.format(pageTemplate, page2Title);
        settings.setUserAgentString(customUserAgentString);
        loadDataSync(
            awContents, onPageFinishedHelper, page1, "text/html", false);
        assertEquals(page1Title + customUserAgentString, getTitleOnUiThread(awContents));
        loadDataSync(
            awContents, onPageFinishedHelper, page2, "text/html", false);
        assertEquals(page2Title + customUserAgentString, getTitleOnUiThread(awContents));
        settings.setUserAgentString(null);
        // Must not cause any changes until the next page loading.
        assertEquals(page2Title + customUserAgentString, getTitleOnUiThread(awContents));
        HistoryUtils.goBackSync(getInstrumentation(), contentView, onPageFinishedHelper);
        assertEquals(page1Title + defaultUserAgentString, getTitleOnUiThread(awContents));
        HistoryUtils.goForwardSync(getInstrumentation(), contentView,
                                   onPageFinishedHelper);
        assertEquals(page2Title + defaultUserAgentString, getTitleOnUiThread(awContents));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUserAgentStringWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsUserAgentStringTestHelper(views.getContainer0(), views.getClient0()),
            new AwSettingsUserAgentStringTestHelper(views.getContainer1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUserAgentWithTestServer() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = getAwSettingsOnUiThread(awContents);
        final String customUserAgentString =
                "testUserAgentWithTestServerUserAgent";

        TestWebServer webServer = null;
        String fileName = null;
        try {
            webServer = new TestWebServer(false);
            final String httpPath = "/testUserAgentWithTestServer.html";
            final String url = webServer.setResponse(httpPath, "foo", null);

            settings.setUserAgentString(customUserAgentString);
            loadUrlSync(awContents,
                        contentClient.getOnPageFinishedHelper(),
                        url);

            assertEquals(1, webServer.getRequestCount(httpPath));
            HttpRequest request = webServer.getLastRequest(httpPath);
            Header[] matchingHeaders = request.getHeaders("User-Agent");
            assertEquals(1, matchingHeaders.length);

            Header header = matchingHeaders[0];
            assertEquals(customUserAgentString, header.getValue());
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDomStorageEnabledWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsDomStorageEnabledTestHelper(views.getContainer0(), views.getClient0()),
            new AwSettingsDomStorageEnabledTestHelper(views.getContainer1(), views.getClient1()));
    }

    // Ideally, these three tests below should be combined into one, or tested using
    // runPerViewSettingsTest. However, it seems the database setting cannot be toggled
    // once set. Filed b/8186497.
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDatabaseInitialValue() throws Throwable {
        TestAwContentsClient client = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(client);
        AwSettingsDatabaseTestHelper helper =
                new AwSettingsDatabaseTestHelper(testContainerView, client);
        helper.ensureSettingHasInitialValue();
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDatabaseEnabled() throws Throwable {
        TestAwContentsClient client = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(client);
        AwSettingsDatabaseTestHelper helper =
                new AwSettingsDatabaseTestHelper(testContainerView, client);
        helper.setAlteredSettingValue();
        helper.ensureSettingHasAlteredValue();
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDatabaseDisabled() throws Throwable {
        TestAwContentsClient client = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(client);
        AwSettingsDatabaseTestHelper helper =
                new AwSettingsDatabaseTestHelper(testContainerView, client);
        helper.setInitialSettingValue();
        helper.ensureSettingHasInitialValue();
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUniversalAccessFromFilesWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsUniversalAccessFromFilesTestHelper(views.getContainer0(),
                views.getClient0()),
            new AwSettingsUniversalAccessFromFilesTestHelper(views.getContainer1(),
                views.getClient1()));
    }

    // This test verifies that local image resources can be loaded from file:
    // URLs regardless of file access state.
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testFileAccessFromFilesImage() throws Throwable {
        final String testFile = "webview/image_access.html";
        assertFileIsReadable(UrlUtils.getTestFilePath(testFile));
        final String imageContainerUrl = UrlUtils.getTestFileUrl(testFile);
        final String imageHeight = "16";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = getAwSettingsOnUiThread(awContents);
        settings.setJavaScriptEnabled(true);
        settings.setAllowUniversalAccessFromFileURLs(false);
        settings.setAllowFileAccessFromFileURLs(false);
        loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), imageContainerUrl);
        assertEquals(imageHeight, getTitleOnUiThread(awContents));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testFileAccessFromFilesIframeWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsFileAccessFromFilesIframeTestHelper(
                views.getContainer0(), views.getClient0()),
            new AwSettingsFileAccessFromFilesIframeTestHelper(
                views.getContainer1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testFileAccessFromFilesXhrWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsFileAccessFromFilesXhrTestHelper(views.getContainer0(),
                views.getClient0()),
            new AwSettingsFileAccessFromFilesXhrTestHelper(views.getContainer1(),
                views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testFileUrlAccessWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsFileUrlAccessTestHelper(views.getContainer0(), views.getClient0(), 0),
            new AwSettingsFileUrlAccessTestHelper(views.getContainer1(), views.getClient1(), 1));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testContentUrlAccessWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsContentUrlAccessTestHelper(views.getContainer0(), views.getClient0(), 0),
            new AwSettingsContentUrlAccessTestHelper(views.getContainer1(), views.getClient1(), 1));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "Navigation"})
    public void testBlockingContentUrlsFromDataUrls() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final String target = "content_from_data";
        final String page = "<html><body>" +
                "<img src=\"" +
                createContentUrl(target) + "\">" +
                "</body></html>";
        resetResourceRequestCountInContentProvider(target);
        loadDataSync(
            awContents,
            contentClient.getOnPageFinishedHelper(),
            page,
            "text/html",
            false);
        ensureResourceRequestCountInContentProvider(target, 0);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "Navigation"})
    public void testContentUrlFromFileWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsContentUrlAccessFromFileTestHelper(
                    views.getContainer0(), views.getClient0(), 0),
            new AwSettingsContentUrlAccessFromFileTestHelper(
                    views.getContainer1(), views.getClient1(), 1));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testBlockNetworkImagesDoesNotBlockDataUrlImage() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final AwSettings settings = getAwSettingsOnUiThread(awContents);
        ImagePageGenerator generator = new ImagePageGenerator(0, false);

        settings.setJavaScriptEnabled(true);
        settings.setImagesEnabled(false);
        loadDataSync(awContents,
                     contentClient.getOnPageFinishedHelper(),
                     generator.getPageSource(),
                     "text/html",
                     false);
        assertEquals(ImagePageGenerator.IMAGE_LOADED_STRING, getTitleOnUiThread(awContents));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testBlockNetworkImagesBlocksNetworkImageAndReloadInPlace() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final AwSettings settings = getAwSettingsOnUiThread(awContents);
        settings.setJavaScriptEnabled(true);
        ImagePageGenerator generator = new ImagePageGenerator(0, false);

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String httpImageUrl = generator.getPageUrl(webServer);

            settings.setImagesEnabled(false);
            loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), httpImageUrl);
            assertEquals(ImagePageGenerator.IMAGE_NOT_LOADED_STRING,
                    getTitleOnUiThread(awContents));

            settings.setImagesEnabled(true);
            poll(new Callable<Boolean>() {
                @Override
                public Boolean call() throws Exception {
                    return ImagePageGenerator.IMAGE_LOADED_STRING.equals(
                        getTitleOnUiThread(awContents));
                }
            });
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testBlockNetworkImagesWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            runPerViewSettingsTest(
                    new AwSettingsImagesEnabledHelper(
                            views.getContainer0(),
                            views.getClient0(),
                            webServer,
                            new ImagePageGenerator(0, true)),
                    new AwSettingsImagesEnabledHelper(
                            views.getContainer1(),
                            views.getClient1(),
                            webServer,
                            new ImagePageGenerator(1, true)));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testBlockNetworkLoadsWithHttpResources() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainer =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainer.getAwContents();
        final AwSettings awSettings = getAwSettingsOnUiThread(awContents);
        awSettings.setJavaScriptEnabled(true);
        ImagePageGenerator generator = new ImagePageGenerator(0, false);

        TestWebServer webServer = null;
        String fileName = null;
        try {
            // Set up http image.
            webServer = new TestWebServer(false);
            final String httpPath = "/image.png";
            final String imageUrl = webServer.setResponseBase64(
                    httpPath, generator.getImageSourceNoAdvance(),
                    CommonResources.getImagePngHeaders(true));

            // Set up file html that loads http iframe.
            String pageHtml = "<img src='" + imageUrl + "' " +
                      "onload=\"document.title='img_onload_fired';\" " +
                      "onerror=\"document.title='img_onerror_fired';\" />";
            Context context = getInstrumentation().getTargetContext();
            fileName = context.getCacheDir() + "/block_network_loads_test.html";
            TestFileUtil.deleteFile(fileName);  // Remove leftover file if any.
            TestFileUtil.createNewHtmlFile(fileName, "unset", pageHtml);

            // Actual test. Blocking should trigger onerror handler.
            awSettings.setBlockNetworkLoads(true);
            loadUrlSync(
                awContents,
                contentClient.getOnPageFinishedHelper(),
                "file:///" + fileName);
            assertEquals(0, webServer.getRequestCount(httpPath));
            assertEquals("img_onerror_fired", getTitleOnUiThread(awContents));

            // Unblock should load normally.
            awSettings.setBlockNetworkLoads(false);
            loadUrlSync(
                awContents,
                contentClient.getOnPageFinishedHelper(),
                "file:///" + fileName);
            assertEquals(1, webServer.getRequestCount(httpPath));
            assertEquals("img_onload_fired", getTitleOnUiThread(awContents));
        } finally {
            if (fileName != null) TestFileUtil.deleteFile(fileName);
            if (webServer != null) webServer.shutdown();
        }
    }

    private static class AudioEvent {
        private CallbackHelper mCallback;
        public AudioEvent(CallbackHelper callback) {
            mCallback = callback;
        }

        @JavascriptInterface
        public void onCanPlay() {
            mCallback.notifyCalled();
        }

        @JavascriptInterface
        public void onError() {
            mCallback.notifyCalled();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testBlockNetworkLoadsWithAudio() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainer =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainer.getAwContents();
        final AwSettings awSettings = getAwSettingsOnUiThread(awContents);
        final CallbackHelper callback = new CallbackHelper();
        awSettings.setJavaScriptEnabled(true);

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String httpPath = "/audio.mp3";
            // Don't care about the response is correct or not, just want
            // to know whether Url is accessed.
            final String audioUrl = webServer.setResponse(httpPath, "1", null);

            String pageHtml = "<html><body><audio controls src='" + audioUrl + "' " +
                    "oncanplay=\"AudioEvent.onCanPlay();\" " +
                    "onerror=\"AudioEvent.onError();\" /> </body></html>";
            // Actual test. Blocking should trigger onerror handler.
            awSettings.setBlockNetworkLoads(true);
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    awContents.addPossiblyUnsafeJavascriptInterface(
                            new AudioEvent(callback), "AudioEvent", null);
                }
            });
            int count = callback.getCallCount();
            loadDataSync(awContents, contentClient.getOnPageFinishedHelper(), pageHtml,
                    "text/html", false);
            callback.waitForCallback(count, 1);
            assertEquals(0, webServer.getRequestCount(httpPath));

            // The below test failed in Nexus Galaxy.
            // See https://code.google.com/p/chromium/issues/detail?id=313463
            // Unblock should load normally.
            /*
            awSettings.setBlockNetworkLoads(false);
            count = callback.getCallCount();
            loadDataSync(awContents, contentClient.getOnPageFinishedHelper(), pageHtml,
                    "text/html", false);
            callback.waitForCallback(count, 1);
            assertTrue(0 != webServer.getRequestCount(httpPath));
            */
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    // Test an assert URL (file:///android_asset/)
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testAssetUrl() throws Throwable {
        // Note: this text needs to be kept in sync with the contents of the html file referenced
        // below.
        final String expectedTitle = "Asset File";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        loadUrlSync(awContents,
                    contentClient.getOnPageFinishedHelper(),
                    "file:///android_asset/asset_file.html");
        assertEquals(expectedTitle, getTitleOnUiThread(awContents));
    }

    // Test a resource URL (file:///android_res/).
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testResourceUrl() throws Throwable {
        // Note: this text needs to be kept in sync with the contents of the html file referenced
        // below.
        final String expectedTitle = "Resource File";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        loadUrlSync(awContents,
                    contentClient.getOnPageFinishedHelper(),
                    "file:///android_res/raw/resource_file.html");
        assertEquals(expectedTitle, getTitleOnUiThread(awContents));
    }

    // Test that the file URL access toggle does not affect asset URLs.
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testFileUrlAccessToggleDoesNotBlockAssetUrls() throws Throwable {
        // Note: this text needs to be kept in sync with the contents of the html file referenced
        // below.
        final String expectedTitle = "Asset File";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final AwSettings settings = getAwSettingsOnUiThread(awContents);
        settings.setAllowFileAccess(false);
        loadUrlSync(awContents,
                    contentClient.getOnPageFinishedHelper(),
                    "file:///android_asset/asset_file.html");
        assertEquals(expectedTitle, getTitleOnUiThread(awContents));
    }

    // Test that the file URL access toggle does not affect resource URLs.
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testFileUrlAccessToggleDoesNotBlockResourceUrls() throws Throwable {
        // Note: this text needs to be kept in sync with the contents of the html file referenced
        // below.
        final String expectedTitle = "Resource File";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final AwSettings settings = getAwSettingsOnUiThread(awContents);
        settings.setAllowFileAccess(false);
        loadUrlSync(awContents,
                    contentClient.getOnPageFinishedHelper(),
                    "file:///android_res/raw/resource_file.html");
        assertEquals(expectedTitle, getTitleOnUiThread(awContents));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testLayoutAlgorithmWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsLayoutAlgorithmTestHelper(views.getContainer0(), views.getClient0()),
            new AwSettingsLayoutAlgorithmTestHelper(views.getContainer1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testTextZoomWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsTextZoomTestHelper(views.getContainer0(), views.getClient0()),
            new AwSettingsTextZoomTestHelper(views.getContainer1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testTextZoomAutosizingWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsTextZoomAutosizingTestHelper(views.getContainer0(), views.getClient0()),
            new AwSettingsTextZoomAutosizingTestHelper(views.getContainer1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testJavaScriptPopupsWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsJavaScriptPopupsTestHelper(views.getContainer0(), views.getClient0()),
            new AwSettingsJavaScriptPopupsTestHelper(views.getContainer1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testCacheMode() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainer =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainer.getAwContents();
        final AwSettings awSettings = getAwSettingsOnUiThread(testContainer.getAwContents());
        clearCacheOnUiThread(awContents, true);

        assertEquals(WebSettings.LOAD_DEFAULT, awSettings.getCacheMode());
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String htmlPath = "/testCacheMode.html";
            final String url = webServer.setResponse(htmlPath, "response", null);
            awSettings.setCacheMode(WebSettings.LOAD_CACHE_ELSE_NETWORK);
            loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), url);
            assertEquals(1, webServer.getRequestCount(htmlPath));
            loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), url);
            assertEquals(1, webServer.getRequestCount(htmlPath));

            awSettings.setCacheMode(WebSettings.LOAD_NO_CACHE);
            loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), url);
            assertEquals(2, webServer.getRequestCount(htmlPath));
            loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), url);
            assertEquals(3, webServer.getRequestCount(htmlPath));

            awSettings.setCacheMode(WebSettings.LOAD_CACHE_ONLY);
            loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), url);
            assertEquals(3, webServer.getRequestCount(htmlPath));
            loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), url);
            assertEquals(3, webServer.getRequestCount(htmlPath));

            final String htmlNotInCachePath = "/testCacheMode-not-in-cache.html";
            final String urlNotInCache = webServer.setResponse(htmlNotInCachePath, "", null);
            loadUrlSyncAndExpectError(awContents,
                    contentClient.getOnPageFinishedHelper(),
                    contentClient.getOnReceivedErrorHelper(),
                    urlNotInCache);
            assertEquals(0, webServer.getRequestCount(htmlNotInCachePath));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    // As our implementation of network loads blocking uses the same net::URLRequest settings, make
    // sure that setting cache mode doesn't accidentally enable network loads.  The reference
    // behaviour is that when network loads are blocked, setting cache mode has no effect.
    public void testCacheModeWithBlockedNetworkLoads() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainer =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainer.getAwContents();
        final AwSettings awSettings = getAwSettingsOnUiThread(testContainer.getAwContents());
        clearCacheOnUiThread(awContents, true);

        assertEquals(WebSettings.LOAD_DEFAULT, awSettings.getCacheMode());
        awSettings.setBlockNetworkLoads(true);
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String htmlPath = "/testCacheModeWithBlockedNetworkLoads.html";
            final String url = webServer.setResponse(htmlPath, "response", null);
            loadUrlSyncAndExpectError(awContents,
                    contentClient.getOnPageFinishedHelper(),
                    contentClient.getOnReceivedErrorHelper(),
                    url);
            assertEquals(0, webServer.getRequestCount(htmlPath));

            awSettings.setCacheMode(WebSettings.LOAD_CACHE_ELSE_NETWORK);
            loadUrlSyncAndExpectError(awContents,
                    contentClient.getOnPageFinishedHelper(),
                    contentClient.getOnReceivedErrorHelper(),
                    url);
            assertEquals(0, webServer.getRequestCount(htmlPath));

            awSettings.setCacheMode(WebSettings.LOAD_NO_CACHE);
            loadUrlSyncAndExpectError(awContents,
                    contentClient.getOnPageFinishedHelper(),
                    contentClient.getOnReceivedErrorHelper(),
                    url);
            assertEquals(0, webServer.getRequestCount(htmlPath));

            awSettings.setCacheMode(WebSettings.LOAD_CACHE_ONLY);
            loadUrlSyncAndExpectError(awContents,
                    contentClient.getOnPageFinishedHelper(),
                    contentClient.getOnReceivedErrorHelper(),
                    url);
            assertEquals(0, webServer.getRequestCount(htmlPath));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testCacheModeWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            runPerViewSettingsTest(
                    new AwSettingsCacheModeTestHelper(
                            views.getContainer0(), views.getClient0(), 0, webServer),
                    new AwSettingsCacheModeTestHelper(
                            views.getContainer1(), views.getClient1(), 1, webServer));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    static class ManifestTestHelper {
        private final TestWebServer mWebServer;
        private final String mHtmlPath;
        private final String mHtmlUrl;
        private final String mManifestPath;

        ManifestTestHelper(
                TestWebServer webServer, String htmlPageName, String manifestName) {
            mWebServer = webServer;
            mHtmlPath = "/" + htmlPageName;
            mHtmlUrl = webServer.setResponse(
                    mHtmlPath, "<html manifest=\"" + manifestName + "\"></html>", null);
            mManifestPath = "/" + manifestName;
            webServer.setResponse(
                    mManifestPath,
                    "CACHE MANIFEST",
                    CommonResources.getContentTypeAndCacheHeaders("text/cache-manifest", false));
        }

        String getHtmlPath() {
            return mHtmlPath;
        }

        String getHtmlUrl() {
            return mHtmlUrl;
        }

        String getManifestPath() {
            return mManifestPath;
        }

        int waitUntilHtmlIsRequested(final int initialRequestCount) throws Exception {
            return waitUntilResourceIsRequested(mHtmlPath, initialRequestCount);
        }

        int waitUntilManifestIsRequested(final int initialRequestCount) throws Exception {
            return waitUntilResourceIsRequested(mManifestPath, initialRequestCount);
        }

        private int waitUntilResourceIsRequested(
                final String path, final int initialRequestCount) throws Exception {
            poll(new Callable<Boolean>() {
                @Override
                public Boolean call() throws Exception {
                    return mWebServer.getRequestCount(path) > initialRequestCount;
                }
            });
            return mWebServer.getRequestCount(path);
        }
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "AppCache"})
    public void testAppCache() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainer =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainer.getAwContents();
        final AwSettings settings = getAwSettingsOnUiThread(awContents);
        settings.setJavaScriptEnabled(true);
        // Note that the cache isn't actually enabled until the call to setAppCachePath.
        settings.setAppCacheEnabled(true);

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            ManifestTestHelper helper = new ManifestTestHelper(
                    webServer, "testAppCache.html", "appcache.manifest");
            loadUrlSync(
                    awContents,
                    contentClient.getOnPageFinishedHelper(),
                    helper.getHtmlUrl());
            helper.waitUntilHtmlIsRequested(0);
            // Unfortunately, there is no other good way of verifying that AppCache is
            // disabled, other than checking that it didn't try to fetch the manifest.
            Thread.sleep(1000);
            assertEquals(0, webServer.getRequestCount(helper.getManifestPath()));
            settings.setAppCachePath("whatever");  // Enables AppCache.
            loadUrlSync(
                    awContents,
                    contentClient.getOnPageFinishedHelper(),
                    helper.getHtmlUrl());
            helper.waitUntilManifestIsRequested(0);
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "AppCache"})
    public void testAppCacheWithTwoViews() throws Throwable {
        // We don't use the test helper here, because making sure that AppCache
        // is disabled takes a lot of time, so running through the usual drill
        // will take about 20 seconds.
        ViewPair views = createViews();

        AwSettings settings0 = getAwSettingsOnUiThread(views.getContents0());
        settings0.setJavaScriptEnabled(true);
        settings0.setAppCachePath("whatever");
        settings0.setAppCacheEnabled(true);
        AwSettings settings1 = getAwSettingsOnUiThread(views.getContents1());
        settings1.setJavaScriptEnabled(true);
        // AppCachePath setting is global, no need to set it for the second view.
        settings1.setAppCacheEnabled(true);

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            ManifestTestHelper helper0 = new ManifestTestHelper(
                    webServer, "testAppCache_0.html", "appcache.manifest_0");
            loadUrlSync(
                    views.getContents0(),
                    views.getClient0().getOnPageFinishedHelper(),
                    helper0.getHtmlUrl());
            int manifestRequests0 = helper0.waitUntilManifestIsRequested(0);
            ManifestTestHelper helper1 = new ManifestTestHelper(
                    webServer, "testAppCache_1.html", "appcache.manifest_1");
            loadUrlSync(
                    views.getContents1(),
                    views.getClient1().getOnPageFinishedHelper(),
                    helper1.getHtmlUrl());
            helper1.waitUntilManifestIsRequested(0);
            settings1.setAppCacheEnabled(false);
            loadUrlSync(
                    views.getContents0(),
                    views.getClient0().getOnPageFinishedHelper(),
                    helper0.getHtmlUrl());
            helper0.waitUntilManifestIsRequested(manifestRequests0);
            final int prevManifestRequestCount =
                    webServer.getRequestCount(helper1.getManifestPath());
            int htmlRequests1 = webServer.getRequestCount(helper1.getHtmlPath());
            loadUrlSync(
                    views.getContents1(),
                    views.getClient1().getOnPageFinishedHelper(),
                    helper1.getHtmlUrl());
            helper1.waitUntilHtmlIsRequested(htmlRequests1);
            // Unfortunately, there is no other good way of verifying that AppCache is
            // disabled, other than checking that it didn't try to fetch the manifest.
            Thread.sleep(1000);
            assertEquals(
                    prevManifestRequestCount, webServer.getRequestCount(helper1.getManifestPath()));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUseWideViewportWithTwoViews() throws Throwable {
        ViewPair views = createViews(true);
        runPerViewSettingsTest(
            new AwSettingsUseWideViewportTestHelper(views.getContainer0(), views.getClient0()),
            new AwSettingsUseWideViewportTestHelper(views.getContainer1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUseWideViewportWithTwoViewsNoQuirks() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsUseWideViewportTestHelper(views.getContainer0(), views.getClient0()),
            new AwSettingsUseWideViewportTestHelper(views.getContainer1(), views.getClient1()));
    }

    private void useWideViewportLayoutWidthTest(
            AwTestContainerView testContainer, CallbackHelper onPageFinishedHelper)
            throws Throwable {
        final AwContents awContents = testContainer.getAwContents();
        AwSettings settings = getAwSettingsOnUiThread(awContents);

        final String pageTemplate = "<html><head>%s</head>" +
                "<body onload='document.title=document.body.clientWidth'></body></html>";
        final String pageNoViewport = String.format(pageTemplate, "");
        final String pageViewportDeviceWidth = String.format(
                pageTemplate,
                "<meta name='viewport' content='width=device-width' />");
        final String viewportTagSpecifiedWidth = "3000";
        final String pageViewportSpecifiedWidth = String.format(
                pageTemplate,
                "<meta name='viewport' content='width=" + viewportTagSpecifiedWidth + "' />");

        DeviceDisplayInfo deviceInfo = DeviceDisplayInfo.create(testContainer.getContext());
        int displayWidth = (int) (deviceInfo.getDisplayWidth() / deviceInfo.getDIPScale());

        settings.setJavaScriptEnabled(true);
        assertFalse(settings.getUseWideViewPort());
        // When UseWideViewPort is off, "width" setting of "meta viewport"
        // tags is ignored, and the layout width is set to device width in CSS pixels.
        // Thus, all 3 pages will have the same body width.
        loadDataSync(awContents, onPageFinishedHelper, pageNoViewport, "text/html", false);
        int actualWidth = Integer.parseInt(getTitleOnUiThread(awContents));
        // Avoid rounding errors.
        assertTrue("Expected: " + displayWidth + ", Actual: " + actualWidth,
                Math.abs(displayWidth - actualWidth) <= 1);
        loadDataSync(awContents, onPageFinishedHelper, pageViewportDeviceWidth, "text/html", false);
        actualWidth = Integer.parseInt(getTitleOnUiThread(awContents));
        assertTrue("Expected: " + displayWidth + ", Actual: " + actualWidth,
                Math.abs(displayWidth - actualWidth) <= 1);
        loadDataSync(
                awContents, onPageFinishedHelper, pageViewportSpecifiedWidth, "text/html", false);
        actualWidth = Integer.parseInt(getTitleOnUiThread(awContents));
        assertTrue("Expected: " + displayWidth + ", Actual: " + actualWidth,
                Math.abs(displayWidth - actualWidth) <= 1);

        settings.setUseWideViewPort(true);
        // When UseWideViewPort is on, "meta viewport" tag is used.
        // If there is no viewport tag, or width isn't specified,
        // then layout width is set to max(980, <device-width-in-DIP-pixels>)
        loadDataSync(awContents, onPageFinishedHelper, pageNoViewport, "text/html", false);
        actualWidth = Integer.parseInt(getTitleOnUiThread(awContents));
        assertTrue("Expected: >= 980 , Actual: " + actualWidth, actualWidth >= 980);
        loadDataSync(awContents, onPageFinishedHelper, pageViewportDeviceWidth, "text/html", false);
        actualWidth = Integer.parseInt(getTitleOnUiThread(awContents));
        assertTrue("Expected: " + displayWidth + ", Actual: " + actualWidth,
                Math.abs(displayWidth - actualWidth) <= 1);
        loadDataSync(
                awContents, onPageFinishedHelper, pageViewportSpecifiedWidth, "text/html", false);
        assertEquals(viewportTagSpecifiedWidth, getTitleOnUiThread(awContents));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUseWideViewportLayoutWidth() throws Throwable {
        TestAwContentsClient contentClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        useWideViewportLayoutWidthTest(testContainerView, contentClient.getOnPageFinishedHelper());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUseWideViewportLayoutWidthNoQuirks() throws Throwable {
        TestAwContentsClient contentClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient, false);
        useWideViewportLayoutWidthTest(testContainerView, contentClient.getOnPageFinishedHelper());
    }

    @MediumTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUseWideViewportControlsDoubleTabToZoom() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();
        AwSettings settings = getAwSettingsOnUiThread(awContents);
        settings.setBuiltInZoomControls(true);

        DeviceDisplayInfo deviceInfo =
                DeviceDisplayInfo.create(testContainerView.getContext());
        int displayWidth = (int) (deviceInfo.getDisplayWidth() / deviceInfo.getDIPScale());
        int layoutWidth = displayWidth * 2;
        final String page = "<html>" +
                "<head><meta name='viewport' content='width=" + layoutWidth + "'>" +
                "<style> body { width: " + layoutWidth + "px; }</style></head>" +
                "<body>Page Text</body></html>";

        assertFalse(settings.getUseWideViewPort());
        // Without wide viewport the <meta viewport> tag will be ignored by WebView,
        // but it doesn't really matter as we don't expect double tap to change the scale.
        loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        final float initialScale = getScaleOnUiThread(awContents);
        simulateDoubleTapCenterOfWebViewOnUiThread(testContainerView);
        Thread.sleep(1000);
        assertEquals(initialScale, getScaleOnUiThread(awContents));

        settings.setUseWideViewPort(true);
        loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        int onScaleChangedCallCount = contentClient.getOnScaleChangedHelper().getCallCount();
        simulateDoubleTapCenterOfWebViewOnUiThread(testContainerView);
        contentClient.getOnScaleChangedHelper().waitForCallback(onScaleChangedCallCount);
        final float zoomedOutScale = getScaleOnUiThread(awContents);
        assertTrue("zoomedOut: " + zoomedOutScale + ", initial: " + initialScale,
                zoomedOutScale < initialScale);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testForceZeroLayoutHeightWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsForceZeroLayoutHeightTestHelper(
                        views.getContainer0(), views.getClient0(), false),
                new AwSettingsForceZeroLayoutHeightTestHelper(
                        views.getContainer1(), views.getClient1(), false));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testForceZeroLayoutHeightViewportTagWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsForceZeroLayoutHeightTestHelper(
                        views.getContainer0(), views.getClient0(), true),
                new AwSettingsForceZeroLayoutHeightTestHelper(
                        views.getContainer1(), views.getClient1(), true));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testZeroLayoutHeightDisablesViewportQuirkWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsZeroLayoutHeightDisablesViewportQuirkTestHelper(
                        views.getContainer0(), views.getClient0()),
                new AwSettingsZeroLayoutHeightDisablesViewportQuirkTestHelper(
                        views.getContainer1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testLoadWithOverviewModeWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsLoadWithOverviewModeTestHelper(
                        views.getContainer0(), views.getClient0(), false),
                new AwSettingsLoadWithOverviewModeTestHelper(
                        views.getContainer1(), views.getClient1(), false));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testLoadWithOverviewModeViewportTagWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsLoadWithOverviewModeTestHelper(
                        views.getContainer0(), views.getClient0(), true),
                new AwSettingsLoadWithOverviewModeTestHelper(
                        views.getContainer1(), views.getClient1(), true));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testSetInitialScale() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final AwSettings awSettings = getAwSettingsOnUiThread(awContents);
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();

        WindowManager wm = (WindowManager) getInstrumentation().getTargetContext()
                .getSystemService(Context.WINDOW_SERVICE);
        Point screenSize = new Point();
        wm.getDefaultDisplay().getSize(screenSize);
        // Make sure after 50% scale, page width still larger than screen.
        int height = screenSize.y * 2 + 1;
        int width = screenSize.x * 2 + 1;
        final String page = "<html><body>" +
                "<p style='height:" + height + "px;width:" + width + "px'>" +
                "testSetInitialScale</p></body></html>";
        final float defaultScale =
            getInstrumentation().getTargetContext().getResources().getDisplayMetrics().density;

        assertEquals(defaultScale, getPixelScaleOnUiThread(awContents), .01f);
        loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        assertEquals(defaultScale, getPixelScaleOnUiThread(awContents), .01f);

        int onScaleChangedCallCount = contentClient.getOnScaleChangedHelper().getCallCount();
        awSettings.setInitialPageScale(50);
        loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        contentClient.getOnScaleChangedHelper().waitForCallback(onScaleChangedCallCount);
        assertEquals(0.5f, getPixelScaleOnUiThread(awContents), .01f);

        onScaleChangedCallCount = contentClient.getOnScaleChangedHelper().getCallCount();
        awSettings.setInitialPageScale(500);
        loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        contentClient.getOnScaleChangedHelper().waitForCallback(onScaleChangedCallCount);
        assertEquals(5.0f, getPixelScaleOnUiThread(awContents), .01f);

        onScaleChangedCallCount = contentClient.getOnScaleChangedHelper().getCallCount();
        awSettings.setInitialPageScale(0);
        loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        contentClient.getOnScaleChangedHelper().waitForCallback(onScaleChangedCallCount);
        assertEquals(defaultScale, getPixelScaleOnUiThread(awContents), .01f);
    }

    /*
    @LargeTest
    @Feature({"AndroidWebView", "Preferences"})
    http://crbug.com/304549
    */
    @DisabledTest
    public void testMediaPlaybackWithoutUserGesture() throws Throwable {
        assertTrue(VideoTestUtil.runVideoTest(this, false, WAIT_TIMEOUT_MS));
    }

    /*
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    http://crbug.com/396645
    */
    @DisabledTest
    public void testMediaPlaybackWithUserGesture() throws Throwable {
        // Wait for 5 second to see if video played.
        assertFalse(VideoTestUtil.runVideoTest(this, true, scaleTimeout(5000)));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDefaultVideoPosterURL() throws Throwable {
        final CallbackHelper videoPosterAccessedCallbackHelper = new CallbackHelper();
        final String DEFAULT_VIDEO_POSTER_URL = "http://default_video_poster/";
        TestAwContentsClient client = new TestAwContentsClient() {
            @Override
            public AwWebResourceResponse shouldInterceptRequest(
                    ShouldInterceptRequestParams params) {
                if (params.url.equals(DEFAULT_VIDEO_POSTER_URL)) {
                    videoPosterAccessedCallbackHelper.notifyCalled();
                }
                return null;
            }
        };
        final AwContents awContents = createAwTestContainerViewOnMainSync(client).getAwContents();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                AwSettings awSettings = awContents.getSettings();
                awSettings.setDefaultVideoPosterURL(DEFAULT_VIDEO_POSTER_URL);
            }
        });
        VideoTestWebServer webServer = new VideoTestWebServer(
                getInstrumentation().getTargetContext());
        try {
            String data = "<html><head><body>" +
                "<video id='video' control src='" +
                webServer.getOnePixelOneFrameWebmURL() + "' /> </body></html>";
            loadDataAsync(awContents, data, "text/html", false);
            videoPosterAccessedCallbackHelper.waitForCallback(0, 1, 20, TimeUnit.SECONDS);
        } finally {
            if (webServer.getTestWebServer() != null)
                webServer.getTestWebServer().shutdown();
        }
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testAllowMixedMode() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient() {
            @Override
            public void onReceivedSslError(ValueCallback<Boolean> callback, SslError error) {
                callback.onReceiveValue(true);
            }
        };
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final AwSettings awSettings = getAwSettingsOnUiThread(awContents);

        awSettings.setJavaScriptEnabled(true);

        TestWebServer httpsServer = new TestWebServer(true);
        TestWebServer httpServer = new TestWebServer(false);

        final String JS_URL = "/insecure.js";
        final String IMG_URL = "/insecure.png";
        final String SECURE_URL = "/secure.html";
        httpServer.setResponse(JS_URL, "window.loaded_js = 42;", null);
        httpServer.setResponseBase64(IMG_URL, CommonResources.FAVICON_DATA_BASE64, null);

        final String JS_HTML = "<script src=\"" + httpServer.getResponseUrl(JS_URL) +
                "\"></script>";
        final String IMG_HTML = "<img src=\"" + httpServer.getResponseUrl(IMG_URL) + "\" />";
        final String SECURE_HTML = "<body>" + IMG_HTML + " " + JS_HTML + "</body>";

        String secureUrl = httpsServer.setResponse(SECURE_URL, SECURE_HTML, null);

        awSettings.setMixedContentMode(AwSettings.MIXED_CONTENT_NEVER_ALLOW);
        loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), secureUrl);
        assertEquals(1, httpsServer.getRequestCount(SECURE_URL));
        assertEquals(0, httpServer.getRequestCount(JS_URL));
        assertEquals(0, httpServer.getRequestCount(IMG_URL));

        awSettings.setMixedContentMode(AwSettings.MIXED_CONTENT_ALWAYS_ALLOW);
        loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), secureUrl);
        assertEquals(2, httpsServer.getRequestCount(SECURE_URL));
        assertEquals(1, httpServer.getRequestCount(JS_URL));
        assertEquals(1, httpServer.getRequestCount(IMG_URL));

        awSettings.setMixedContentMode(AwSettings.MIXED_CONTENT_COMPATIBILITY_MODE);
        loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), secureUrl);
        assertEquals(3, httpsServer.getRequestCount(SECURE_URL));
        assertEquals(1, httpServer.getRequestCount(JS_URL));
        assertEquals(2, httpServer.getRequestCount(IMG_URL));

        httpServer.shutdown();
        httpsServer.shutdown();
    }


    static class ViewPair {
        private final AwTestContainerView mContainer0;
        private final TestAwContentsClient mClient0;
        private final AwTestContainerView mContainer1;
        private final TestAwContentsClient mClient1;

        ViewPair(AwTestContainerView container0, TestAwContentsClient client0,
                 AwTestContainerView container1, TestAwContentsClient client1) {
            this.mContainer0 = container0;
            this.mClient0 = client0;
            this.mContainer1 = container1;
            this.mClient1 = client1;
        }

        AwTestContainerView getContainer0() {
            return mContainer0;
        }

        AwContents getContents0() {
            return mContainer0.getAwContents();
        }

        TestAwContentsClient getClient0() {
            return mClient0;
        }

        AwTestContainerView getContainer1() {
            return mContainer1;
        }

        AwContents getContents1() {
            return mContainer1.getAwContents();
        }

        TestAwContentsClient getClient1() {
            return mClient1;
        }
    }

    /**
     * Verifies the following statements about a setting:
     *  - initially, the setting has a default value;
     *  - the setting can be switched to an alternate value and back;
     *  - switching a setting in the first WebView doesn't affect the setting
     *    state in the second WebView and vice versa.
     *
     * @param helper0 Test helper for the first ContentView
     * @param helper1 Test helper for the second ContentView
     * @throws Throwable
     */
    private void runPerViewSettingsTest(AwSettingsTestHelper helper0,
                                        AwSettingsTestHelper helper1) throws Throwable {
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasInitialValue();

        helper1.setAlteredSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasAlteredValue();

        helper1.setInitialSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasInitialValue();

        helper0.setAlteredSettingValue();
        helper0.ensureSettingHasAlteredValue();
        helper1.ensureSettingHasInitialValue();

        helper0.setInitialSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasInitialValue();

        helper0.setAlteredSettingValue();
        helper0.ensureSettingHasAlteredValue();
        helper1.ensureSettingHasInitialValue();

        helper1.setAlteredSettingValue();
        helper0.ensureSettingHasAlteredValue();
        helper1.ensureSettingHasAlteredValue();

        helper0.setInitialSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasAlteredValue();

        helper1.setInitialSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasInitialValue();
    }

    private ViewPair createViews() throws Throwable {
        return createViews(false);
    }

    private ViewPair createViews(boolean supportsLegacyQuirks) throws Throwable {
        TestAwContentsClient client0 = new TestAwContentsClient();
        TestAwContentsClient client1 = new TestAwContentsClient();
        return new ViewPair(
            createAwTestContainerViewOnMainSync(client0, supportsLegacyQuirks),
            client0,
            createAwTestContainerViewOnMainSync(client1, supportsLegacyQuirks),
            client1);
    }

    static void assertFileIsReadable(String filePath) {
        File file = new File(filePath);
        try {
            assertTrue("Test file \"" + filePath + "\" is not readable." +
                    "Please make sure that files from android_webview/test/data/device_files/ " +
                    "has been pushed to the device before testing",
                    file.canRead());
        } catch (SecurityException e) {
            fail("Got a SecurityException for \"" + filePath + "\": " + e.toString());
        }
    }

    /**
     * Verifies the number of resource requests made to the content provider.
     * @param resource Resource name
     * @param expectedCount Expected resource requests count
     */
    private void ensureResourceRequestCountInContentProvider(String resource, int expectedCount) {
        Context context = getInstrumentation().getTargetContext();
        int actualCount = TestContentProvider.getResourceRequestCount(context, resource);
        assertEquals(expectedCount, actualCount);
    }

    private void resetResourceRequestCountInContentProvider(String resource) {
        Context context = getInstrumentation().getTargetContext();
        TestContentProvider.resetResourceRequestCount(context, resource);
    }

    private String createContentUrl(final String target) {
        return TestContentProvider.createContentUrl(target);
    }

    private void simulateDoubleTapCenterOfWebViewOnUiThread(final AwTestContainerView webView)
            throws Throwable {
        final int x = (webView.getRight() - webView.getLeft()) / 2;
        final int y = (webView.getBottom() - webView.getTop()) / 2;
        final AwContents awContents = webView.getAwContents();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                awContents.getContentViewCore().sendDoubleTapForTest(
                        SystemClock.uptimeMillis(), x, y);
            }
        });
    }
}
