// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;
import android.webkit.WebSettings;

import org.apache.http.Header;
import org.apache.http.HttpRequest;
import org.chromium.android_webview.AndroidProtocolHandler;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.ImagePageGenerator;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentSettings;
import org.chromium.content.browser.ContentSettings.LayoutAlgorithm;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.HistoryUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.gfx.DeviceDisplayInfo;

import java.util.concurrent.Callable;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.ArrayList;
import java.util.List;

/**
 * A test suite for ContentSettings class. The key objective is to verify that each
 * settings applies either to each individual view or to all views of the
 * application
 */
public class AwSettingsTest extends AndroidWebViewTestBase {
    private static final long TEST_TIMEOUT = 20000L;
    private static final int CHECK_INTERVAL = 100;

    private static final boolean ENABLED = true;
    private static final boolean DISABLED = false;

    /**
     * A helper class for testing a particular preference from ContentSettings.
     * The generic type T is the type of the setting. Usually, to test an
     * effect of the preference, JS code is executed that sets document's title.
     * In this case, requiresJsEnabled constructor argument must be set to true.
     */
    abstract class AwSettingsTestHelper<T> {
        protected final AwContents mAwContents;
        protected final TestAwContentsClient mContentViewClient;
        protected final ContentSettings mContentSettings;

        AwSettingsTestHelper(AwContents awContents,
                             TestAwContentsClient contentViewClient,
                             boolean requiresJsEnabled) throws Throwable {
            mAwContents = awContents;
            mContentViewClient = contentViewClient;
            mContentSettings = getContentSettingsOnUiThread(mAwContents);
            if (requiresJsEnabled) {
                mContentSettings.setJavaScriptEnabled(true);
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

        protected abstract void setCurrentValue(T value);

        protected abstract void doEnsureSettingHasValue(T value) throws Throwable;

        protected String getTitleOnUiThread() throws Throwable {
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

        private void ensureSettingHasValue(T value) throws Throwable {
            assertEquals(value, getCurrentValue());
            doEnsureSettingHasValue(value);
        }
    }

    class AwSettingsJavaScriptTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String JS_ENABLED_STRING = "JS Enabled";
        private static final String JS_DISABLED_STRING = "JS Disabled";

        AwSettingsJavaScriptTestHelper(AwContents awContents,
                                       TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient, false);
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
            return mContentSettings.getJavaScriptEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setJavaScriptEnabled(value);
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
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient);
            // Load the page.
            super.doEnsureSettingHasValue(getInitialValue());
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            String oldTitle = getTitleOnUiThread();
            String newTitle = oldTitle + "_modified";
            executeJavaScriptAndWaitForResult(
                mAwContents, mContentViewClient, getScript(newTitle));
            assertEquals(value == ENABLED ? newTitle : oldTitle, getTitleOnUiThread());
        }

        private String getScript(String title) {
            return "document.title='" + title + "';";
        }
    }

    class AwSettingsPluginsTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String PLUGINS_ENABLED_STRING = "Embed";
        private static final String PLUGINS_DISABLED_STRING = "NoEmbed";

        AwSettingsPluginsTestHelper(AwContents awContents,
                                    TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient, true);
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
            return mContentSettings.getPluginsEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setPluginsEnabled(value);
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
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient, true);
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
            return mContentSettings.getStandardFontFamily();
        }

        @Override
        protected void setCurrentValue(String value) {
            mContentSettings.setStandardFontFamily(value);
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
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient, true);
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
            return mContentSettings.getDefaultFontSize();
        }

        @Override
        protected void setCurrentValue(Integer value) {
            mContentSettings.setDefaultFontSize(value);
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
                AwContents awContents,
                TestAwContentsClient contentViewClient,
                ImagePageGenerator generator) throws Throwable {
            super(awContents, contentViewClient, true);
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
            return mContentSettings.getLoadsImagesAutomatically();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setLoadsImagesAutomatically(value);
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

    class AwSettingsDefaultTextEncodingTestHelper extends AwSettingsTestHelper<String> {
        AwSettingsDefaultTextEncodingTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient, true);
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
            return mContentSettings.getDefaultTextEncodingName();
        }

        @Override
        protected void setCurrentValue(String value) {
            mContentSettings.setDefaultTextEncodingName(value);
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
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient, true);
            mDefaultUa = mContentSettings.getUserAgentString();
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
            String currentUa = mContentSettings.getUserAgentString();
            return mDefaultUa.equals(currentUa) ? DEFAULT_UA : currentUa;
        }

        @Override
        protected void setCurrentValue(String value) {
            mContentSettings.setUserAgentString(value);
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
        private static final String NO_LOCAL_STORAGE = "No localStorage";
        private static final String HAS_LOCAL_STORAGE = "Has localStorage";

        AwSettingsDomStorageEnabledTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient, true);
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
            return mContentSettings.getDomStorageEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setDomStorageEnabled(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            // It is not permitted to access localStorage from data URLs in WebKit,
            // that is why a standalone page must be used.
            loadUrlSync(UrlUtils.getTestFileUrl("webview/localStorage.html"));
            assertEquals(
                value == ENABLED ? HAS_LOCAL_STORAGE : NO_LOCAL_STORAGE,
                getTitleOnUiThread());
        }
    }

    class AwSettingsDatabaseTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String NO_DATABASE = "No database";
        private static final String HAS_DATABASE = "Has database";

        AwSettingsDatabaseTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient, true);
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
            return mContentSettings.getDatabaseEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setDatabaseEnabled(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            // It seems accessing the database through a data scheme is not
            // supported, and fails with a DOM exception (likely a cross-domain
            // violation).
            loadUrlSync(UrlUtils.getTestFileUrl("webview/database_access.html"));
            assertEquals(
                value == ENABLED ? HAS_DATABASE : NO_DATABASE,
                getTitleOnUiThread());
        }
    }

    class AwSettingsUniversalAccessFromFilesTestHelper extends AwSettingsTestHelper<Boolean> {
        // TODO(mnaganov): Change to "Exception" once
        // https://bugs.webkit.org/show_bug.cgi?id=43504 is fixed.
        private static final String ACCESS_DENIED_TITLE = "undefined";

        AwSettingsUniversalAccessFromFilesTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient, true);
            mIframeContainerUrl = UrlUtils.getTestFileUrl("webview/iframe_access.html");
            mIframeUrl = UrlUtils.getTestFileUrl("webview/hello_world.html");
            // The value of the setting depends on the SDK version.
            mContentSettings.setAllowUniversalAccessFromFileURLs(false);
            // If universal access is true, the value of file access doesn't
            // matter. While if universal access is false, having file access
            // enabled will allow file loading.
            mContentSettings.setAllowFileAccessFromFileURLs(false);
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
            return mContentSettings.getAllowUniversalAccessFromFileURLs();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setAllowUniversalAccessFromFileURLs(value);
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
        // TODO(mnaganov): Change to "Exception" once
        // https://bugs.webkit.org/show_bug.cgi?id=43504 is fixed.
        private static final String ACCESS_DENIED_TITLE = "undefined";

        AwSettingsFileAccessFromFilesIframeTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient, true);
            mIframeContainerUrl = UrlUtils.getTestFileUrl("webview/iframe_access.html");
            mIframeUrl = UrlUtils.getTestFileUrl("webview/hello_world.html");
            mContentSettings.setAllowUniversalAccessFromFileURLs(false);
            // The value of the setting depends on the SDK version.
            mContentSettings.setAllowFileAccessFromFileURLs(false);
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
            return mContentSettings.getAllowFileAccessFromFileURLs();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setAllowFileAccessFromFileURLs(value);
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
        private static final String ACCESS_GRANTED_TITLE = "Hello, World!";
        private static final String ACCESS_DENIED_TITLE = "Exception";

        AwSettingsFileAccessFromFilesXhrTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient, true);
            mXhrContainerUrl = UrlUtils.getTestFileUrl("webview/xhr_access.html");
            mContentSettings.setAllowUniversalAccessFromFileURLs(false);
            // The value of the setting depends on the SDK version.
            mContentSettings.setAllowFileAccessFromFileURLs(false);
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
            return mContentSettings.getAllowFileAccessFromFileURLs();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setAllowFileAccessFromFileURLs(value);
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

    // A helper super class for test cases that need to access AwSettings.
    abstract class AwSettingsWithSettingsTestHelper<T> extends AwSettingsTestHelper<T> {
        protected AwSettings mAwSettings;

        AwSettingsWithSettingsTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient,
                boolean requiresJsEnabled) throws Throwable {
            super(awContents, contentViewClient, requiresJsEnabled);
            mAwSettings = AwSettingsTest.this.getAwSettingsOnUiThread(awContents);
        }
    }

    class AwSettingsFileUrlAccessTestHelper extends AwSettingsWithSettingsTestHelper<Boolean> {
        private static final String ACCESS_GRANTED_TITLE = "Hello, World!";

        AwSettingsFileUrlAccessTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient,
                int startIndex) throws Throwable {
            super(awContents, contentViewClient, true);
            mIndex = startIndex;
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
            String fileUrl = UrlUtils.getTestFileUrl("webview/hello_world.html?id=" + mIndex);
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

    class AwSettingsContentUrlAccessTestHelper extends AwSettingsWithSettingsTestHelper<Boolean> {

        AwSettingsContentUrlAccessTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient,
                int index) throws Throwable {
            super(awContents, contentViewClient, true);
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
            loadUrlSync(AwSettingsTest.this.createContentUrl(mTarget));
            if (value == ENABLED) {
                AwSettingsTest.this.ensureResourceRequestCountInContentProvider(mTarget, 1);
            } else {
                AwSettingsTest.this.ensureResourceRequestCountInContentProvider(mTarget, 0);
            }
        }

        private final String mTarget;
    }

    class AwSettingsContentUrlAccessFromFileTestHelper
            extends AwSettingsWithSettingsTestHelper<Boolean> {
        private static final String TARGET = "content_from_file";

        AwSettingsContentUrlAccessFromFileTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient,
                int index) throws Throwable {
            super(awContents, contentViewClient, true);
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
    abstract class AwSettingsTextAutosizingTestHelper<T>
            extends AwSettingsWithSettingsTestHelper<T> {
        protected static final float PARAGRAPH_FONT_SIZE = 14.0f;

        AwSettingsTextAutosizingTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient, true);
        }

        protected float getActualFontSize() throws Throwable {
            loadDataSync(getData());
            // Retrieve font size after the native callback has fired, not in body.onload.
            // The latter can fire prior to Font autosizer adjustments.
            executeJavaScriptAndWaitForResult(
                mAwContents, mContentViewClient, "setTitleToActualFontSize()");
            return Float.parseFloat(getTitleOnUiThread());
        }

        protected String getData() {
            StringBuilder sb = new StringBuilder();
            sb.append("<html>" +
                    "<head><script>" +
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
            for (int i = 0; i < 100; i++) {
                sb.append("Hello, World! ");
            }
            sb.append("</p></body></html>");
            return sb.toString();
        }
    }

    class AwSettingsLayoutAlgorithmTestHelper extends
                                              AwSettingsTextAutosizingTestHelper<LayoutAlgorithm> {

        AwSettingsLayoutAlgorithmTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient);
            // Font autosizing doesn't step in for narrow layout widths.
            mContentSettings.setUseWideViewPort(true);
            mAwSettings.setEnableFixedLayoutMode(true);
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
            return mContentSettings.getLayoutAlgorithm();
        }

        @Override
        protected void setCurrentValue(LayoutAlgorithm value) {
            mContentSettings.setLayoutAlgorithm(value);
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
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient);
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
            return mContentSettings.getTextZoom();
        }

        @Override
        protected void setCurrentValue(Integer value) {
            mContentSettings.setTextZoom(value);
            mAwSettings.setTextZoom(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Integer value) throws Throwable {
            final float actualFontSize = getActualFontSize();
            // Ensure that actual vs. initial font size ratio is similar to actual vs. initial
            // text zoom values ratio.
            final float ratiosDelta = Math.abs(
                (actualFontSize / mInitialActualFontSize) -
                (value / (float)INITIAL_TEXT_ZOOM));
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
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient);
            mContentSettings.setLayoutAlgorithm(LayoutAlgorithm.TEXT_AUTOSIZING);
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
            return mContentSettings.getTextZoom();
        }

        @Override
        protected void setCurrentValue(Integer value) {
            mContentSettings.setTextZoom(value);
            // This is to verify that AwSettings will not affect font boosting by Autosizer.
            mAwSettings.setTextZoom(-1);
        }

        @Override
        protected void doEnsureSettingHasValue(Integer value) throws Throwable {
            final float actualFontSize = getActualFontSize();
            // Ensure that actual vs. initial font size ratio is similar to actual vs. initial
            // text zoom values ratio.
            final float ratiosDelta = Math.abs(
                (actualFontSize / mInitialActualFontSize) -
                (value / (float)INITIAL_TEXT_ZOOM));
            assertTrue(
                "|(" + actualFontSize + " / " + mInitialActualFontSize + ") - (" +
                value + " / " + INITIAL_TEXT_ZOOM + ")| = " + ratiosDelta,
                ratiosDelta <= 0.2f);
        }
    }

    class AwSettingsJavaScriptPopupsTestHelper extends AwSettingsTestHelper<Boolean> {
        static private final String POPUP_ENABLED = "Popup enabled";
        static private final String POPUP_BLOCKED = "Popup blocked";

        AwSettingsJavaScriptPopupsTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient, true);
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
            return mContentSettings.getJavaScriptCanOpenWindowsAutomatically();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setJavaScriptCanOpenWindowsAutomatically(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            final boolean expectPopupEnabled = value;
            assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    try {
                        String title = getTitleOnUiThread();
                        return expectPopupEnabled ? POPUP_ENABLED.equals(title) :
                                POPUP_BLOCKED.equals(title);
                    } catch (Throwable t) {
                        t.printStackTrace();
                        fail("Failed to getTitleOnUiThread: " + t.toString());
                        return false;
                    }
                }
            }, TEST_TIMEOUT, CHECK_INTERVAL));
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

    class AwSettingsCacheModeTestHelper extends AwSettingsWithSettingsTestHelper<Integer> {

        AwSettingsCacheModeTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient,
                int index,
                TestWebServer webServer) throws Throwable {
            super(awContents, contentViewClient, true);
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
    class AwSettingsUseWideViewportTestHelper extends AwSettingsWithSettingsTestHelper<Boolean> {
        static private final String VIEWPORT_TAG_LAYOUT_WIDTH = "3000";

        AwSettingsUseWideViewportTestHelper(
                AwContents awContents,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(awContents, contentViewClient, true);
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
            return mContentSettings.getUseWideViewPort();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setUseWideViewPort(value);
            mAwSettings.setEnableFixedLayoutMode(value);
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
                AwContents awContents,
                TestAwContentsClient contentViewClient,
                boolean withViewPortTag) throws Throwable {
            super(awContents, contentViewClient, true);
            mWithViewPortTag = withViewPortTag;
            mContentSettings.setUseWideViewPort(true);
            mAwContents.getSettings().setEnableFixedLayoutMode(true);
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
            return mContentSettings.getLoadWithOverviewMode();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mExpectScaleChange = mContentSettings.getLoadWithOverviewMode() != value;
            if (mExpectScaleChange) {
                mOnScaleChangedCallCount =
                        mContentViewClient.getOnScaleChangedHelper().getCallCount();
            }
            mContentSettings.setLoadWithOverviewMode(value);
            mAwContents.resetScrollAndScaleState();
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

    // The test verifies that JavaScript is disabled upon WebView
    // creation without accessing ContentSettings. If the test passes,
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
            new AwSettingsJavaScriptTestHelper(views.getContents0(), views.getClient0()),
            new AwSettingsJavaScriptTestHelper(views.getContents1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testJavaScriptEnabledDynamicWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsJavaScriptDynamicTestHelper(views.getContents0(), views.getClient0()),
            new AwSettingsJavaScriptDynamicTestHelper(views.getContents1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testPluginsEnabledWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsPluginsTestHelper(views.getContents0(), views.getClient0()),
            new AwSettingsPluginsTestHelper(views.getContents1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testStandardFontFamilyWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsStandardFontFamilyTestHelper(views.getContents0(), views.getClient0()),
            new AwSettingsStandardFontFamilyTestHelper(views.getContents1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDefaultFontSizeWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsDefaultFontSizeTestHelper(views.getContents0(), views.getClient0()),
            new AwSettingsDefaultFontSizeTestHelper(views.getContents1(), views.getClient1()));
    }

    // The test verifies that disabling images loading by setting
    // setLoadsImagesAutomatically to false doesn't prevent images already
    // contained in the memory cache to be displayed.  The cache is shared among
    // all views, so the image can be put there by another view.
    @Feature({"AndroidWebView", "Preferences"})
    @SmallTest
    public void testLoadsImagesAutomaticallyWithCachedImage() throws Throwable {
        ViewPair views = createViews();
        ContentSettings settings0 = getContentSettingsOnUiThread(views.getContents0());
        settings0.setJavaScriptEnabled(true);
        ContentSettings settings1 = getContentSettingsOnUiThread(views.getContents1());
        settings1.setJavaScriptEnabled(true);
        ImagePageGenerator generator = new ImagePageGenerator(0, false);

        // First disable images loading and verify it.
        settings0.setLoadsImagesAutomatically(false);
        settings1.setLoadsImagesAutomatically(false);
        loadDataSync(views.getContents0(),
                     views.getClient0().getOnPageFinishedHelper(),
                     generator.getPageSource(),
                     "text/html", false);
        loadDataSync(views.getContents1(),
                     views.getClient1().getOnPageFinishedHelper(),
                     generator.getPageSource(),
                     "text/html", false);
        assertEquals(ImagePageGenerator.IMAGE_NOT_LOADED_STRING,
                getTitleOnUiThread(views.getContents0()));
        assertEquals(ImagePageGenerator.IMAGE_NOT_LOADED_STRING,
                getTitleOnUiThread(views.getContents1()));

        // Now enable images loading only for view 0.
        settings0.setLoadsImagesAutomatically(true);
        loadDataSync(views.getContents0(),
                     views.getClient0().getOnPageFinishedHelper(),
                     generator.getPageSource(),
                     "text/html", false);
        loadDataSync(views.getContents1(),
                     views.getClient1().getOnPageFinishedHelper(),
                     generator.getPageSource(),
                     "text/html", false);

        // Once the image has been cached by contentView0, it is available to contentView1.
        assertEquals(ImagePageGenerator.IMAGE_LOADED_STRING,
                getTitleOnUiThread(views.getContents0()));
        assertEquals(ImagePageGenerator.IMAGE_LOADED_STRING,
                getTitleOnUiThread(views.getContents1()));
        ImagePageGenerator generator1 = new ImagePageGenerator(1, false);

        // This is a new image. view 1 can't load it.
        loadDataSync(views.getContents1(),
                     views.getClient1().getOnPageFinishedHelper(),
                     generator1.getPageSource(),
                     "text/html", false);
        assertEquals(ImagePageGenerator.IMAGE_NOT_LOADED_STRING,
                     getTitleOnUiThread(views.getContents1()));
        loadDataSync(views.getContents0(),
                     views.getClient0().getOnPageFinishedHelper(),
                     generator1.getPageSource(),
                     "text/html", false);
        loadDataSync(views.getContents1(),
                     views.getClient1().getOnPageFinishedHelper(),
                     generator1.getPageSource(),
                     "text/html", false);

        // Once the image has been cached by contentViewCore0, it is available to contentViewCore1.
        assertEquals(ImagePageGenerator.IMAGE_LOADED_STRING,
                getTitleOnUiThread(views.getContents0()));
        assertEquals(ImagePageGenerator.IMAGE_LOADED_STRING,
                getTitleOnUiThread(views.getContents1()));
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
        ContentSettings settings = getContentSettingsOnUiThread(awContents);
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
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return !ImagePageGenerator.IMAGE_NOT_LOADED_STRING.equals(
                            getTitleOnUiThread(awContents));
                } catch (Throwable t) {
                    t.printStackTrace();
                    fail("Failed to getTitleOnUiThread: " + t.toString());
                    return false;
                }
            }
        }, TEST_TIMEOUT, CHECK_INTERVAL));
        assertEquals(ImagePageGenerator.IMAGE_LOADED_STRING, getTitleOnUiThread(awContents));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testLoadsImagesAutomaticallyWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsLoadImagesAutomaticallyTestHelper(
                views.getContents0(), views.getClient0(), new ImagePageGenerator(0, true)),
            new AwSettingsLoadImagesAutomaticallyTestHelper(
                views.getContents1(), views.getClient1(), new ImagePageGenerator(1, true)));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDefaultTextEncodingWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsDefaultTextEncodingTestHelper(views.getContents0(), views.getClient0()),
            new AwSettingsDefaultTextEncodingTestHelper(views.getContents1(), views.getClient1()));
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
        ContentSettings settings = getContentSettingsOnUiThread(awContents);
        final String actualUserAgentString = settings.getUserAgentString();
        final String patternString =
                "Mozilla/5\\.0 \\(Linux;( U;)? Android ([^;]+);( (\\w+)-(\\w+);)?" +
                "\\s?(.*)\\sBuild/(.+)\\) AppleWebKit/(\\d+)\\.(\\d+) \\(KHTML, like Gecko\\) " +
                "Version/\\d+\\.\\d+( Mobile)? Safari/(\\d+)\\.(\\d+)";
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
        ContentSettings settings = getContentSettingsOnUiThread(awContents);
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
        final String customUserAgentString = "ContentSettingsTest";
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
        ContentSettings settings = getContentSettingsOnUiThread(awContents);
        settings.setJavaScriptEnabled(true);
        final String defaultUserAgentString = settings.getUserAgentString();
        final String customUserAgentString = "ContentSettingsTest";
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
            new AwSettingsUserAgentStringTestHelper(views.getContents0(), views.getClient0()),
            new AwSettingsUserAgentStringTestHelper(views.getContents1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUserAgentWithTestServer() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        AwContents awContents = testContainerView.getAwContents();
        ContentSettings settings = getContentSettingsOnUiThread(awContents);
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
            new AwSettingsDomStorageEnabledTestHelper(views.getContents0(), views.getClient0()),
            new AwSettingsDomStorageEnabledTestHelper(views.getContents1(), views.getClient1()));
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
        final AwContents awContents = testContainerView.getAwContents();
        AwSettingsDatabaseTestHelper helper = new AwSettingsDatabaseTestHelper(awContents, client);
        helper.ensureSettingHasInitialValue();
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDatabaseEnabled() throws Throwable {
        TestAwContentsClient client = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettingsDatabaseTestHelper helper = new AwSettingsDatabaseTestHelper(awContents, client);
        helper.setAlteredSettingValue();
        helper.ensureSettingHasAlteredValue();
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDatabaseDisabled() throws Throwable {
        TestAwContentsClient client = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettingsDatabaseTestHelper helper = new AwSettingsDatabaseTestHelper(awContents, client);
        helper.setInitialSettingValue();
        helper.ensureSettingHasInitialValue();
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUniversalAccessFromFilesWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsUniversalAccessFromFilesTestHelper(views.getContents0(),
                views.getClient0()),
            new AwSettingsUniversalAccessFromFilesTestHelper(views.getContents1(),
                views.getClient1()));
    }

    // This test verifies that local image resources can be loaded from file:
    // URLs regardless of file access state.
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testFileAccessFromFilesImage() throws Throwable {
        final String imageContainerUrl = UrlUtils.getTestFileUrl("webview/image_access.html");
        final String imageHeight = "16";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        ContentSettings settings = getContentSettingsOnUiThread(awContents);
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
                views.getContents0(), views.getClient0()),
            new AwSettingsFileAccessFromFilesIframeTestHelper(
                views.getContents1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testFileAccessFromFilesXhrWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsFileAccessFromFilesXhrTestHelper(views.getContents0(),
                views.getClient0()),
            new AwSettingsFileAccessFromFilesXhrTestHelper(views.getContents1(),
                views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testFileUrlAccessWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsFileUrlAccessTestHelper(views.getContents0(), views.getClient0(), 0),
            new AwSettingsFileUrlAccessTestHelper(views.getContents1(), views.getClient1(), 1));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testContentUrlAccessWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsContentUrlAccessTestHelper(views.getContents0(), views.getClient0(), 0),
            new AwSettingsContentUrlAccessTestHelper(views.getContents1(), views.getClient1(), 1));
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
                    views.getContents0(), views.getClient0(), 0),
            new AwSettingsContentUrlAccessFromFileTestHelper(
                    views.getContents1(), views.getClient1(), 1));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testBlockNetworkImagesDoesNotBlockDataUrlImage() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final ContentSettings settings = getContentSettingsOnUiThread(awContents);
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
        final ContentSettings settings = getContentSettingsOnUiThread(awContents);
        settings.setJavaScriptEnabled(true);
        ImagePageGenerator generator = new ImagePageGenerator(0, false);

        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);
            final String imagePath = "/image.png";
            webServer.setResponseBase64(imagePath, generator.getImageSourceNoAdvance(),
                    CommonResources.getImagePngHeaders(false));

            final String pagePath = "/html_image.html";
            final String httpUrlImageHtml = generator.getPageTemplateSource(imagePath);
            final String httpImageUrl = webServer.setResponse(pagePath, httpUrlImageHtml, null);

            settings.setImagesEnabled(false);
            loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), httpImageUrl);
            assertEquals(ImagePageGenerator.IMAGE_NOT_LOADED_STRING,
                    getTitleOnUiThread(awContents));

            settings.setImagesEnabled(true);
            assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    try {
                        return ImagePageGenerator.IMAGE_NOT_LOADED_STRING.equals(
                            getTitleOnUiThread(awContents));
                    } catch (Throwable t) {
                        t.printStackTrace();
                        fail("Failed to getTitleOnUIThread: " + t.toString());
                        return false;
                    }
                }
            }, TEST_TIMEOUT, CHECK_INTERVAL));
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
        final ContentSettings contentSettings = getContentSettingsOnUiThread(awContents);
        final AwSettings awSettings = getAwSettingsOnUiThread(testContainer.getAwContents());
        contentSettings.setJavaScriptEnabled(true);
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
            String pageHtml ="<img src='" + imageUrl + "' " +
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
        try {
            useTestResourceContext();
            loadUrlSync(awContents,
                        contentClient.getOnPageFinishedHelper(),
                        "file:///android_asset/asset_file.html");
            assertEquals(expectedTitle, getTitleOnUiThread(awContents));
        } finally {
            resetResourceContext();
        }
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
        try {
            useTestResourceContext();
            loadUrlSync(awContents,
                        contentClient.getOnPageFinishedHelper(),
                        "file:///android_res/raw/resource_file.html");
            assertEquals(expectedTitle, getTitleOnUiThread(awContents));
        } finally {
            resetResourceContext();
        }
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
        try {
            useTestResourceContext();
            settings.setAllowFileAccess(false);
            loadUrlSync(awContents,
                        contentClient.getOnPageFinishedHelper(),
                        "file:///android_asset/asset_file.html");
            assertEquals(expectedTitle, getTitleOnUiThread(awContents));
        } finally {
            resetResourceContext();
        }
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
        try {
            useTestResourceContext();
            settings.setAllowFileAccess(false);
            loadUrlSync(awContents,
                        contentClient.getOnPageFinishedHelper(),
                        "file:///android_res/raw/resource_file.html");
            assertEquals(expectedTitle, getTitleOnUiThread(awContents));
        } finally {
            resetResourceContext();
        }
    }

    /*
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    http://crbug.com/171492
    */
    @DisabledTest
    public void testLayoutAlgorithmWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsLayoutAlgorithmTestHelper(views.getContents0(), views.getClient0()),
            new AwSettingsLayoutAlgorithmTestHelper(views.getContents1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testTextZoomWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsTextZoomTestHelper(views.getContents0(), views.getClient0()),
            new AwSettingsTextZoomTestHelper(views.getContents1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testTextZoomAutosizingWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsTextZoomAutosizingTestHelper(views.getContents0(), views.getClient0()),
            new AwSettingsTextZoomAutosizingTestHelper(views.getContents1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testJavaScriptPopupsWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsJavaScriptPopupsTestHelper(views.getContents0(), views.getClient0()),
            new AwSettingsJavaScriptPopupsTestHelper(views.getContents1(), views.getClient1()));
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
                            views.getContents0(), views.getClient0(), 0, webServer),
                    new AwSettingsCacheModeTestHelper(
                            views.getContents1(), views.getClient1(), 1, webServer));
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

        int waitUntilHtmlIsRequested(final int initialRequestCount) throws InterruptedException {
            return waitUntilResourceIsRequested(mHtmlPath, initialRequestCount);
        }

        int waitUntilManifestIsRequested(final int initialRequestCount)
                throws InterruptedException {
            return waitUntilResourceIsRequested(mManifestPath, initialRequestCount);
        }

        private int waitUntilResourceIsRequested(
                final String path, final int initialRequestCount) throws InterruptedException {
            assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mWebServer.getRequestCount(path) > initialRequestCount;
                }
            }, TEST_TIMEOUT, CHECK_INTERVAL));
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
        final ContentSettings settings = getContentSettingsOnUiThread(awContents);
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

    /*
     * @SmallTest
     * @Feature({"AndroidWebView", "Preferences", "AppCache"})
     * This test is flaky but the root cause is not found yet. See crbug.com/171765.
     */
    @DisabledTest
    public void testAppCacheWithTwoViews() throws Throwable {
        // We don't use the test helper here, because making sure that AppCache
        // is disabled takes a lot of time, so running through the usual drill
        // will take about 20 seconds.
        ViewPair views = createViews();

        ContentSettings settings0 = getContentSettingsOnUiThread(views.getContents0());
        settings0.setJavaScriptEnabled(true);
        settings0.setAppCachePath("whatever");
        settings0.setAppCacheEnabled(true);
        ContentSettings settings1 = getContentSettingsOnUiThread(views.getContents1());
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
        ViewPair views = createViews();
        runPerViewSettingsTest(
            new AwSettingsUseWideViewportTestHelper(views.getContents0(), views.getClient0()),
            new AwSettingsUseWideViewportTestHelper(views.getContents1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUseWideViewportLayoutWidth() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        ContentSettings settings = getContentSettingsOnUiThread(awContents);
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();

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

        DeviceDisplayInfo deviceInfo =
                DeviceDisplayInfo.create(getInstrumentation().getTargetContext());
        int displayWidth = (int) (deviceInfo.getDisplayWidth() / deviceInfo.getDIPScale());

        settings.setJavaScriptEnabled(true);
        assertFalse(settings.getUseWideViewPort());
        // When UseWideViewPort is off, "meta viewport" tags are ignored,
        // and the layout width is set to device width in CSS pixels.
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
        awContents.getSettings().setEnableFixedLayoutMode(true);
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
    public void testLoadWithOverviewModeWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsLoadWithOverviewModeTestHelper(
                        views.getContents0(), views.getClient0(), false),
                new AwSettingsLoadWithOverviewModeTestHelper(
                        views.getContents1(), views.getClient1(), false));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testLoadWithOverviewModeViewportTagWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsLoadWithOverviewModeTestHelper(
                        views.getContents0(), views.getClient0(), true),
                new AwSettingsLoadWithOverviewModeTestHelper(
                        views.getContents1(), views.getClient1(), true));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    // Verify that LoadViewOverviewMode doesn't affect pages with initial scale
    // set in the viewport tag.
    public void testLoadWithOverviewModeViewportScale() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        ContentSettings settings = getContentSettingsOnUiThread(awContents);
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();

        final int pageScale = 2;
        final String page = "<html><head>" +
                "<meta name='viewport' content='width=3000, initial-scale=" + pageScale +
                "' /></head>" +
                "<body></body></html>";

        assertFalse(settings.getUseWideViewPort());
        assertFalse(settings.getLoadWithOverviewMode());
        loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        assertEquals(1.0f, getScaleOnUiThread(awContents));

        settings.setUseWideViewPort(true);
        awContents.getSettings().setEnableFixedLayoutMode(true);
        settings.setLoadWithOverviewMode(true);
        awContents.resetScrollAndScaleState();
        int onScaleChangedCallCount = contentClient.getOnScaleChangedHelper().getCallCount();
        loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        contentClient.getOnScaleChangedHelper().waitForCallback(onScaleChangedCallCount);
        assertEquals((float)pageScale, getScaleOnUiThread(awContents));
    }

    static class ViewPair {
        private final AwContents contents0;
        private final TestAwContentsClient client0;
        private final AwContents contents1;
        private final TestAwContentsClient client1;

        ViewPair(AwContents contents0, TestAwContentsClient client0,
                 AwContents contents1, TestAwContentsClient client1) {
            this.contents0 = contents0;
            this.client0 = client0;
            this.contents1 = contents1;
            this.client1 = client1;
        }

        AwContents getContents0() {
            return contents0;
        }

        TestAwContentsClient getClient0() {
            return client0;
        }

        AwContents getContents1() {
            return contents1;
        }

        TestAwContentsClient getClient1() {
            return client1;
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
        TestAwContentsClient client0 = new TestAwContentsClient();
        TestAwContentsClient client1 = new TestAwContentsClient();
        return new ViewPair(
            createAwTestContainerViewOnMainSync(client0).getAwContents(),
            client0,
            createAwTestContainerViewOnMainSync(client1).getAwContents(),
            client1);
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

    /**
     * Configure the browser to load resources from the test harness instead of the browser
     * application.
     */
    private void useTestResourceContext() {
        AndroidProtocolHandler.setResourceContextForTesting(getInstrumentation().getContext());
    }

    /**
     * Configure the browser to load resources from the browser application.
     */
    private void resetResourceContext() {
        AndroidProtocolHandler.setResourceContextForTesting(null);
    }

    private float getScaleOnUiThread(final AwContents awContents) throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<Float>() {
            @Override
            public Float call() throws Exception {
                return awContents.getContentViewCore().getScale();
            }
        });
    }
}
