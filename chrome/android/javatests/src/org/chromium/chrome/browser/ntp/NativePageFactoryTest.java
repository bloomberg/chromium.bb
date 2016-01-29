// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;

import org.chromium.chrome.browser.NativePage;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.NativePageFactory.NativePageType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Tests public methods in NativePageFactory.
 */
public class NativePageFactoryTest extends InstrumentationTestCase {

    private static class MockNativePage implements NativePage {
        public final NativePageType type;
        public int updateForUrlCalls;

        public MockNativePage(NativePageType type) {
            this.type = type;
        }

        @Override
        public void updateForUrl(String url) {
            updateForUrlCalls++;
        }

        @Override
        public String getUrl() {
            return null;
        }

        @Override
        public String getHost() {
            switch (type) {
                case NTP:
                    return UrlConstants.NTP_HOST;
                case BOOKMARKS:
                    return UrlConstants.BOOKMARKS_HOST;
                case RECENT_TABS:
                    return UrlConstants.RECENT_TABS_HOST;
                default:
                    fail("Unexpected NativePageType: " + type);
                    return null;
            }
        }

        @Override
        public void destroy() {}

        @Override
        public String getTitle() {
            return null;
        }

        @Override
        public int getBackgroundColor() {
            return 0;
        }

        @Override
        public int getThemeColor() {
            return 0;
        }

        @Override
        public View getView() {
            return null;
        }
    }

    private static class MockNativePageBuilder extends NativePageFactory.NativePageBuilder {
        @Override
        public NativePage buildNewTabPage(Activity activity, Tab tab,
                TabModelSelector tabModelSelector) {
            return new MockNativePage(NativePageType.NTP);
        }

        @Override
        public NativePage buildBookmarksPage(Activity activity, Tab tab) {
            return new MockNativePage(NativePageType.BOOKMARKS);
        }

        @Override
        public NativePage buildRecentTabsPage(Activity activity, Tab tab) {
            return new MockNativePage(NativePageType.RECENT_TABS);
        }
    }

    private static class UrlCombo {
        public String url;
        public NativePageType expectedType;

        public UrlCombo(String url, NativePageType expectedType) {
            this.url = url;
            this.expectedType = expectedType;
        }
    }

    private static final UrlCombo[] VALID_URLS = {
        new UrlCombo("chrome-native://newtab", NativePageType.NTP),
        new UrlCombo("chrome-native://newtab/", NativePageType.NTP),
        new UrlCombo("chrome-native://bookmarks", NativePageType.BOOKMARKS),
        new UrlCombo("chrome-native://bookmarks/", NativePageType.BOOKMARKS),
        new UrlCombo("chrome-native://bookmarks/#245", NativePageType.BOOKMARKS),
        new UrlCombo("chrome-native://recent-tabs", NativePageType.RECENT_TABS),
        new UrlCombo("chrome-native://recent-tabs/", NativePageType.RECENT_TABS),
    };

    private static final String[] INVALID_URLS = {
        null,
        "",
        "newtab",
        "newtab@google.com:80",
        "/newtab",
        "://newtab",
        "chrome://",
        "chrome://newtab",
        "chrome://newtab#bookmarks",
        "chrome://newtab/#open_tabs",
        "chrome://recent-tabs",
        "chrome://most_visited",
        "chrome-native://",
        "chrome-native://newtablet",
        "chrome-native://bookmarks-inc",
        "chrome-native://recent_tabs",
        "chrome-native://recent-tabswitcher",
        "chrome-native://most_visited",
        "chrome-native://astronaut",
        "chrome-internal://newtab",
        "french-fries://newtab",
        "http://bookmarks",
        "https://recent-tabs",
        "newtab://recent-tabs",
        "recent-tabs bookmarks",
    };

    private boolean isValidInIncognito(UrlCombo urlCombo) {
        return urlCombo.expectedType != NativePageType.RECENT_TABS;
    }

    /**
     * Ensures that NativePageFactory.isNativePageUrl() returns true for native page URLs.
     */
    private void runTestPostiveIsNativePageUrl() {
        for (UrlCombo urlCombo : VALID_URLS) {
            String url = urlCombo.url;
            assertTrue(url, NativePageFactory.isNativePageUrl(url, false));
            if (isValidInIncognito(urlCombo)) {
                assertTrue(url, NativePageFactory.isNativePageUrl(url, true));
            }
        }
    }

    /**
     * Ensures that NativePageFactory.isNativePageUrl() returns false for URLs that don't
     * correspond to a native page.
     */
    private void runTestNegativeIsNativePageUrl() {
        for (String invalidUrl : INVALID_URLS) {
            assertFalse(invalidUrl, NativePageFactory.isNativePageUrl(invalidUrl, false));
            assertFalse(invalidUrl, NativePageFactory.isNativePageUrl(invalidUrl, true));
        }
    }

    /**
     * Ensures that NativePageFactory.createNativePageForURL() returns a native page of the right
     * type and reuses the candidate page if it's the right type.
     */
    private void runTestCreateNativePage() {
        NativePageType[] candidateTypes = new NativePageType[] { NativePageType.NONE,
            NativePageType.NTP, NativePageType.BOOKMARKS, NativePageType.RECENT_TABS };
        for (boolean isIncognito : new boolean[] {true, false}) {
            for (UrlCombo urlCombo : VALID_URLS) {
                if (isIncognito && !isValidInIncognito(urlCombo)) continue;
                for (NativePageType candidateType : candidateTypes) {
                    MockNativePage candidate = candidateType == NativePageType.NONE ? null
                            : new MockNativePage(candidateType);
                    MockNativePage page = (MockNativePage) NativePageFactory.createNativePageForURL(
                            urlCombo.url, candidate, null, null, null, isIncognito);
                    String debugMessage = String.format(
                            "Failed test case: isIncognito=%s, urlCombo={%s,%s}, candidateType=%s",
                            isIncognito, urlCombo.url, urlCombo.expectedType, candidateType);
                    assertNotNull(debugMessage, page);
                    assertEquals(debugMessage, 1, page.updateForUrlCalls);
                    assertEquals(debugMessage, urlCombo.expectedType, page.type);
                    if (candidateType == urlCombo.expectedType) {
                        assertSame(debugMessage, candidate, page);
                    } else {
                        assertNotSame(debugMessage, candidate, page);
                    }
                }
            }
        }
    }

    /**
     * Ensures that NativePageFactory.createNativePageForURL() returns null for URLs that don't
     * correspond to a native page.
     */
    private void runTestCreateNativePageWithInvalidUrl() {
        for (UrlCombo urlCombo : VALID_URLS) {
            if (!isValidInIncognito(urlCombo)) {
                assertNull(urlCombo.url, NativePageFactory.createNativePageForURL(urlCombo.url,
                        null, null, null, null, true));
            }
        }
        for (boolean isIncognito : new boolean[] {true, false}) {
            for (String invalidUrl : INVALID_URLS) {
                assertNull(invalidUrl, NativePageFactory.createNativePageForURL(invalidUrl, null,
                        null, null, null, isIncognito));
            }
        }
    }

    /**
     * Runs all the runTest* methods defined above.
     */
    @SmallTest
    @UiThreadTest
    public void testNativePageFactory() {
        NativePageFactory.setNativePageBuilderForTesting(new MockNativePageBuilder());
        runTestPostiveIsNativePageUrl();
        runTestNegativeIsNativePageUrl();
        runTestCreateNativePage();
        runTestCreateNativePageWithInvalidUrl();
    }
}
