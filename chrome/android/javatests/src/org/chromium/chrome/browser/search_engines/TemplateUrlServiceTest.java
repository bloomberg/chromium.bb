// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.LoadListener;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrl;
import org.chromium.chrome.test.util.ApplicationData;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for Chrome on Android's usage of the TemplateUrlService API.
 */
public class TemplateUrlServiceTest extends NativeLibraryTestBase {

    private static final String QUERY_PARAMETER = "q";
    private static final String QUERY_VALUE = "cat";

    private static final String ALTERNATIVE_PARAMETER = "ctxsl_alternate_term";
    private static final String ALTERNATIVE_VALUE = "lion";

    private static final String VERSION_PARAMETER = "ctxs";
    private static final String VERSION_VALUE_TWO_REQUEST_PROTOCOL = "2";
    private static final String VERSION_VALUE_SINGLE_REQUEST_PROTOCOL = "3";

    private static final String PREFETCH_PARAMETER = "pf";
    private static final String PREFETCH_VALUE = "c";

    @Override
    public void setUp() throws Exception {
        super.setUp();
        ApplicationData.clearAppData(getInstrumentation().getTargetContext());
        loadNativeLibraryAndInitBrowserProcess();
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testUrlForContextualSearchQueryValid() throws ExecutionException {
        waitForTemplateUrlServiceToLoad();

        assertTrue(ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return TemplateUrlService.getInstance().isLoaded();
            }
        }));

        validateQuery(QUERY_VALUE, ALTERNATIVE_VALUE, true, VERSION_VALUE_TWO_REQUEST_PROTOCOL);
        validateQuery(QUERY_VALUE, ALTERNATIVE_VALUE, false, VERSION_VALUE_TWO_REQUEST_PROTOCOL);
        validateQuery(QUERY_VALUE, null, true, VERSION_VALUE_TWO_REQUEST_PROTOCOL);
        validateQuery(QUERY_VALUE, null, false, VERSION_VALUE_TWO_REQUEST_PROTOCOL);
        validateQuery(QUERY_VALUE, null, true, VERSION_VALUE_SINGLE_REQUEST_PROTOCOL);
    }

    private void validateQuery(final String query, final String alternative, final boolean prefetch,
            final String protocolVersion)
            throws ExecutionException {
        String result = ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() throws Exception {
                return TemplateUrlService.getInstance().getUrlForContextualSearchQuery(
                        query, alternative, prefetch, protocolVersion);
            }
        });
        assertNotNull(result);
        Uri uri = Uri.parse(result);
        assertEquals(query, uri.getQueryParameter(QUERY_PARAMETER));
        assertEquals(alternative, uri.getQueryParameter(ALTERNATIVE_PARAMETER));
        assertEquals(protocolVersion, uri.getQueryParameter(VERSION_PARAMETER));
        if (prefetch) {
            assertEquals(PREFETCH_VALUE, uri.getQueryParameter(PREFETCH_PARAMETER));
        } else {
            assertNull(uri.getQueryParameter(PREFETCH_PARAMETER));
        }
    }

    @SmallTest
    @Feature({"SearchEngines"})
    @RetryOnFailure
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE) // see crbug.com/581268
    public void testLoadUrlService() {
        assertFalse(ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return TemplateUrlService.getInstance().isLoaded();
            }
        }));

        waitForTemplateUrlServiceToLoad();

        assertTrue(ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return TemplateUrlService.getInstance().isLoaded();
            }
        }));
    }

    @SmallTest
    @Feature({"SearchEngines"})
    public void testSetAndGetSearchEngine() {
        final TemplateUrlService templateUrlService = waitForTemplateUrlServiceToLoad();

        List<TemplateUrl> searchEngines =
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<List<TemplateUrl>>() {
                    @Override
                    public List<TemplateUrl> call() throws Exception {
                        return templateUrlService.getSearchEngines();
                    }
                });
        // Ensure known state of default search index before running test.
        String searchEngineKeyword =
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<String>() {
                    @Override
                    public String call() throws Exception {
                        return templateUrlService.getDefaultSearchEngineTemplateUrl().getKeyword();
                    }
                });
        assertEquals(searchEngines.get(0).getKeyword(), searchEngineKeyword);

        // Set search engine index and verified it stuck.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                List<TemplateUrl> searchEngines = templateUrlService.getSearchEngines();
                assertTrue("There must be more than one search engine to change searchEngines",
                        searchEngines.size() > 1);
                templateUrlService.setSearchEngine(searchEngines.get(1).getKeyword());
            }
        });
        searchEngineKeyword = ThreadUtils.runOnUiThreadBlockingNoException(new Callable<String>() {
            @Override
            public String call() throws Exception {
                return templateUrlService.getDefaultSearchEngineTemplateUrl().getKeyword();
            }
        });
        assertEquals(searchEngines.get(1).getKeyword(), searchEngineKeyword);
    }

    @SmallTest
    @Feature({"SearchEngines"})
    public void testSortandGetCustomSearchEngine() {
        final TemplateUrlService templateUrlService = waitForTemplateUrlServiceToLoad();

        // Get the number of prepopulated search engine.
        final int prepopulatedEngineNum =
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
                    @Override
                    public Integer call() throws Exception {
                        return templateUrlService.getSearchEngines().size();
                    }
                });

        // Add custom search engines and verified only engines visited within 2 days are added.
        // Also verified custom engines are sorted correctly.
        List<TemplateUrl> customSearchEngines =
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<List<TemplateUrl>>() {
                    @Override
                    public List<TemplateUrl> call() throws Exception {
                        templateUrlService.addSearchEngineForTesting("keyword1", 0);
                        templateUrlService.addSearchEngineForTesting("keyword2", 0);
                        templateUrlService.addSearchEngineForTesting("keyword3", 3);
                        List<TemplateUrl> searchEngines = templateUrlService.getSearchEngines();
                        return searchEngines.subList(prepopulatedEngineNum, searchEngines.size());
                    }
                });
        assertEquals(2, customSearchEngines.size());
        assertEquals("keyword2", customSearchEngines.get(0).getKeyword());
        assertEquals("keyword1", customSearchEngines.get(1).getKeyword());

        // Add more custom search engines and verified at most 3 custom engines are returned.
        // Also verified custom engines are sorted correctly.
        customSearchEngines =
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<List<TemplateUrl>>() {
                    @Override
                    public List<TemplateUrl> call() throws Exception {
                        templateUrlService.addSearchEngineForTesting("keyword4", 0);
                        templateUrlService.addSearchEngineForTesting("keyword5", 0);
                        List<TemplateUrl> searchEngines = templateUrlService.getSearchEngines();
                        return searchEngines.subList(prepopulatedEngineNum, searchEngines.size());
                    }
                });
        assertEquals(3, customSearchEngines.size());
        assertEquals("keyword5", customSearchEngines.get(0).getKeyword());
        assertEquals("keyword4", customSearchEngines.get(1).getKeyword());
        assertEquals("keyword2", customSearchEngines.get(2).getKeyword());

        // Verified last_visited is updated correctly and sorting in descending order correctly.
        customSearchEngines =
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<List<TemplateUrl>>() {
                    @Override
                    public List<TemplateUrl> call() throws Exception {
                        templateUrlService.updateLastVisitedForTesting("keyword3");
                        List<TemplateUrl> searchEngines = templateUrlService.getSearchEngines();
                        return searchEngines.subList(prepopulatedEngineNum, searchEngines.size());
                    }
                });
        assertEquals(3, customSearchEngines.size());
        assertEquals("keyword3", customSearchEngines.get(0).getKeyword());
        assertEquals("keyword5", customSearchEngines.get(1).getKeyword());
        assertEquals("keyword4", customSearchEngines.get(2).getKeyword());

        // Set a custom engine as default provider and verified still 3 custom engines are returned.
        // Also verified custom engines are sorted correctly.
        customSearchEngines =
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<List<TemplateUrl>>() {
                    @Override
                    public List<TemplateUrl> call() throws Exception {
                        templateUrlService.setSearchEngine("keyword4");
                        List<TemplateUrl> searchEngines = templateUrlService.getSearchEngines();
                        return searchEngines.subList(prepopulatedEngineNum, searchEngines.size());
                    }
                });
        assertEquals(4, customSearchEngines.size());
        assertEquals("keyword4", customSearchEngines.get(0).getKeyword());
        assertEquals("keyword3", customSearchEngines.get(1).getKeyword());
        assertEquals("keyword5", customSearchEngines.get(2).getKeyword());
        assertEquals("keyword2", customSearchEngines.get(3).getKeyword());
    }

    private TemplateUrlService waitForTemplateUrlServiceToLoad() {
        final AtomicBoolean observerNotified = new AtomicBoolean(false);
        final LoadListener listener = new LoadListener() {
            @Override
            public void onTemplateUrlServiceLoaded() {
                observerNotified.set(true);
            }
        };
        final TemplateUrlService templateUrlService = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<TemplateUrlService>() {
                    @Override
                    public TemplateUrlService call() {
                        TemplateUrlService service = TemplateUrlService.getInstance();
                        service.registerLoadListener(listener);
                        service.load();
                        return service;
                    }
                });

        CriteriaHelper.pollInstrumentationThread(new Criteria(
                "Observer wasn't notified of TemplateUrlService load.") {
            @Override
            public boolean isSatisfied() {
                return observerNotified.get();
            }
        });
        return templateUrlService;
    }
}
