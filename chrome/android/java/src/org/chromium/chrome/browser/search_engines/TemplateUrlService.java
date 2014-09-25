// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import org.chromium.base.CalledByNative;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Android wrapper of the TemplateUrlService which provides access from the Java
 * layer.
 *
 * Only usable from the UI thread as it's primary purpose is for supporting the Android
 * preferences UI.
 *
 * See components/search_engines/template_url_service.h for more details.
 */
public class TemplateUrlService {

    /**
     * This listener will be notified when template url service is done loading.
     */
    public interface LoadListener {
        public abstract void onTemplateUrlServiceLoaded();
    }

    /**
     * Represents search engine with its index.
     */
    public static class TemplateUrl {
        private final int mIndex;
        private final String mShortName;
        private final String mKeyword;

        @CalledByNative("TemplateUrl")
        public static TemplateUrl create(int id, String shortName, String keyword) {
            return new TemplateUrl(id, shortName, keyword);
        }

        public TemplateUrl(int index, String shortName, String keyword) {
            mIndex = index;
            mShortName = shortName;
            mKeyword = keyword;
        }

        public int getIndex() {
            return mIndex;
        }

        public String getShortName() {
            return mShortName;
        }

        public String getKeyword() {
            return mKeyword;
        }
    }

    private static TemplateUrlService sService;

    public static TemplateUrlService getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sService == null) {
            sService = new TemplateUrlService();
        }
        return sService;
    }

    private final long mNativeTemplateUrlServiceAndroid;
    private final ObserverList<LoadListener> mLoadListeners = new ObserverList<LoadListener>();

    private TemplateUrlService() {
        // Note that this technically leaks the native object, however, TemlateUrlService
        // is a singleton that lives forever and there's no clean shutdown of Chrome on Android
        mNativeTemplateUrlServiceAndroid = nativeInit();
    }

    public boolean isLoaded() {
        ThreadUtils.assertOnUiThread();
        return nativeIsLoaded(mNativeTemplateUrlServiceAndroid);
    }

    public void load() {
        ThreadUtils.assertOnUiThread();
        nativeLoad(mNativeTemplateUrlServiceAndroid);
    }

    /**
     * Get the collection of localized search engines.
     */
    public List<TemplateUrl> getLocalizedSearchEngines() {
        ThreadUtils.assertOnUiThread();
        int templateUrlCount = nativeGetTemplateUrlCount(mNativeTemplateUrlServiceAndroid);
        List<TemplateUrl> templateUrls = new ArrayList<TemplateUrl>(templateUrlCount);
        for (int i = 0; i < templateUrlCount; i++) {
            TemplateUrl templateUrl = nativeGetPrepopulatedTemplateUrlAt(
                    mNativeTemplateUrlServiceAndroid, i);
            if (templateUrl != null) {
                templateUrls.add(templateUrl);
            }
        }
        return templateUrls;
    }

    /**
     * Called from native when template URL service is done loading.
     */
    @CalledByNative
    private void templateUrlServiceLoaded() {
        ThreadUtils.assertOnUiThread();
        for (LoadListener listener : mLoadListeners) {
            listener.onTemplateUrlServiceLoaded();
        }
    }

    /**
     * @return The default search engine index (e.g., 0, 1, 2,...).
     */
    public int getDefaultSearchEngineIndex() {
        ThreadUtils.assertOnUiThread();
        return nativeGetDefaultSearchProvider(mNativeTemplateUrlServiceAndroid);
    }

    /**
     * @return {@link TemplateUrlService.TemplateUrl} for the default search engine.
     */
    public TemplateUrl getDefaultSearchEngineTemplateUrl() {
        if (!isLoaded()) return null;

        int defaultSearchEngineIndex = getDefaultSearchEngineIndex();
        if (defaultSearchEngineIndex == -1) return null;

        assert defaultSearchEngineIndex >= 0;
        assert defaultSearchEngineIndex < nativeGetTemplateUrlCount(
                mNativeTemplateUrlServiceAndroid);

        return nativeGetPrepopulatedTemplateUrlAt(
                mNativeTemplateUrlServiceAndroid, defaultSearchEngineIndex);
    }

    public void setSearchEngine(int selectedIndex) {
        ThreadUtils.assertOnUiThread();
        nativeSetUserSelectedDefaultSearchProvider(mNativeTemplateUrlServiceAndroid, selectedIndex);
    }

    public boolean isSearchProviderManaged() {
        return nativeIsSearchProviderManaged(mNativeTemplateUrlServiceAndroid);
    }

    /**
     * @return Whether or not the default search engine has search by image support.
     */
    public boolean isSearchByImageAvailable() {
        ThreadUtils.assertOnUiThread();
        return nativeIsSearchByImageAvailable(mNativeTemplateUrlServiceAndroid);
    }

    /**
     * @return Whether the default configured search engine is for a Google property.
     */
    public boolean isDefaultSearchEngineGoogle() {
        return nativeIsDefaultSearchEngineGoogle(mNativeTemplateUrlServiceAndroid);
    }

    /**
     * Registers a listener for the callback that indicates that the
     * TemplateURLService has loaded.
     */
    public void registerLoadListener(LoadListener listener) {
        ThreadUtils.assertOnUiThread();
        boolean added = mLoadListeners.addObserver(listener);
        assert added;
    }

    /**
     * Unregisters a listener for the callback that indicates that the
     * TemplateURLService has loaded.
     */
    public void unregisterLoadListener(LoadListener listener) {
        ThreadUtils.assertOnUiThread();
        boolean removed = mLoadListeners.removeObserver(listener);
        assert removed;
    }

    /**
     * Finds the default search engine for the default provider and returns the url query
     * {@link String} for {@code query}.
     * @param query The {@link String} that represents the text query the search url should
     *              represent.
     * @return      A {@link String} that contains the url of the default search engine with
     *              {@code query} inserted as the search parameter.
     */
    public String getUrlForSearchQuery(String query) {
        return nativeGetUrlForSearchQuery(mNativeTemplateUrlServiceAndroid, query);
    }

    /**
     * Finds the default search engine for the default provider and returns the url query
     * {@link String} for {@code query} with voice input source param set.
     * @param query The {@link String} that represents the text query the search url should
     *              represent.
     * @return      A {@link String} that contains the url of the default search engine with
     *              {@code query} inserted as the search parameter and voice input source param set.
     */
    public String getUrlForVoiceSearchQuery(String query) {
        return nativeGetUrlForVoiceSearchQuery(mNativeTemplateUrlServiceAndroid, query);
    }

    /**
     * Replaces the search terms from {@code query} in {@code url}.
     * @param query The {@link String} that represents the text query that should replace the
     *              existing query in {@code url}.
     * @param url   The {@link String} that contains the search url with another search query that
     *              will be replaced with {@code query}.
     * @return      A new version of {@code url} with the search term replaced with {@code query}.
     */
    public String replaceSearchTermsInUrl(String query, String url) {
        return nativeReplaceSearchTermsInUrl(mNativeTemplateUrlServiceAndroid, query, url);
    }

    // TODO(donnd): Delete once the client no longer references it.
    /**
     * Finds the default search engine for the default provider and returns the url query
     * {@link String} for {@code query} with the contextual search version param set.
     * @param query The search term to use as the main query in the returned search url.
     * @param alternateTerm The alternate search term to use as an alternate suggestion.
     * @return      A {@link String} that contains the url of the default search engine with
     *              {@code query} and {@code alternateTerm} inserted as parameters and contextual
     *              search and prefetch parameters set.
     */
     public String getUrlForContextualSearchQuery(String query, String alternateTerm) {
        return nativeGetUrlForContextualSearchQuery(
            mNativeTemplateUrlServiceAndroid, query, alternateTerm, true);
    }

    /**
     * Finds the default search engine for the default provider and returns the url query
     * {@link String} for {@code query} with the contextual search version param set.
     * @param query The search term to use as the main query in the returned search url.
     * @param alternateTerm The alternate search term to use as an alternate suggestion.
     * @param shouldPrefetch Whether the returned url should include a prefetch parameter.
     * @return      A {@link String} that contains the url of the default search engine with
     *              {@code query} and {@code alternateTerm} inserted as parameters and contextual
     *              search and prefetch parameters conditionally set.
     */
     public String getUrlForContextualSearchQuery(String query, String alternateTerm,
            boolean shouldPrefetch) {
        return nativeGetUrlForContextualSearchQuery(
            mNativeTemplateUrlServiceAndroid, query, alternateTerm, shouldPrefetch);
    }

    private native long nativeInit();
    private native void nativeLoad(long nativeTemplateUrlServiceAndroid);
    private native boolean nativeIsLoaded(long nativeTemplateUrlServiceAndroid);
    private native int nativeGetTemplateUrlCount(long nativeTemplateUrlServiceAndroid);
    private native TemplateUrl nativeGetPrepopulatedTemplateUrlAt(
            long nativeTemplateUrlServiceAndroid, int i);
    private native void nativeSetUserSelectedDefaultSearchProvider(
            long nativeTemplateUrlServiceAndroid, int selectedIndex);
    private native int nativeGetDefaultSearchProvider(long nativeTemplateUrlServiceAndroid);
    private native boolean nativeIsSearchProviderManaged(long nativeTemplateUrlServiceAndroid);
    private native boolean nativeIsSearchByImageAvailable(long nativeTemplateUrlServiceAndroid);
    private native boolean nativeIsDefaultSearchEngineGoogle(long nativeTemplateUrlServiceAndroid);
    private native String nativeGetUrlForSearchQuery(long nativeTemplateUrlServiceAndroid,
            String query);
    private native String nativeGetUrlForVoiceSearchQuery(long nativeTemplateUrlServiceAndroid,
            String query);
    private native String nativeReplaceSearchTermsInUrl(long nativeTemplateUrlServiceAndroid,
            String query, String currentUrl);
    private native String nativeGetUrlForContextualSearchQuery(long nativeTemplateUrlServiceAndroid,
            String query, String alternateTerm, boolean shouldPrefetch);
}
