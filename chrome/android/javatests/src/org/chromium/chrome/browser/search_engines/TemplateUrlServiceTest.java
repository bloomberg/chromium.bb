// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.LoadListener;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrl;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for Chrome on Android's usage of the TemplateUrlService API.
 */
public class TemplateUrlServiceTest extends ChromeShellTestBase {

    @Override
    public void setUp() throws Exception {
        super.setUp();
        clearAppData();
        startChromeBrowserProcessSync(getInstrumentation().getTargetContext());
    }

    @SmallTest
    @Feature({"SearchEngines"})
    public void testLoadUrlService() throws InterruptedException {
        final AtomicBoolean loadedResult = new AtomicBoolean();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                loadedResult.set(TemplateUrlService.getInstance().isLoaded());
            }
        });
        assertFalse(loadedResult.get());

        waitForTemplateUrlServiceToLoad();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                loadedResult.set(TemplateUrlService.getInstance().isLoaded());

            }
        });
        assertTrue(loadedResult.get());
    }

    @SmallTest
    @Feature({"SearchEngines"})
    public void testSetAndGetSearchEngine() throws InterruptedException {
        final TemplateUrlService templateUrlService = waitForTemplateUrlServiceToLoad();
        final AtomicInteger searchEngineIndex = new AtomicInteger();

        // Ensure known state of default search index before running teest.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                searchEngineIndex.set(templateUrlService.getDefaultSearchEngineIndex());
            }
        });
        assertEquals(0, searchEngineIndex.get());

        // Set search engine index and verified it stuck.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                List<TemplateUrl> searchEngines =
                        templateUrlService.getLocalizedSearchEngines();
                assertTrue("There must be more than one search engine to change searchEngines",
                        searchEngines.size() > 1);
                templateUrlService.setSearchEngine(1);
                searchEngineIndex.set(templateUrlService.getDefaultSearchEngineIndex());
            }
        });
        assertEquals(1, searchEngineIndex.get());
    }

    private TemplateUrlService waitForTemplateUrlServiceToLoad() throws InterruptedException {
        final AtomicBoolean observerNotified = new AtomicBoolean(false);
        final LoadListener listener = new LoadListener() {
            @Override
            public void onTemplateUrlServiceLoaded() {
                observerNotified.set(true);
            }
        };
        final AtomicReference<TemplateUrlService> templateUrlService =
                new AtomicReference<TemplateUrlService>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TemplateUrlService service = TemplateUrlService.getInstance();
                templateUrlService.set(service);

                service.registerLoadListener(listener);
                service.load();
            }
        });

        assertTrue("Observer wasn't notified of TemplateUrlService load.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return observerNotified.get();
                    }
        }));
        return templateUrlService.get();
    }
}
