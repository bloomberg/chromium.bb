// Copyright (c) 2013 The Chromium Authors. All rights reserved.
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
 * See chrome/browser/search_engines/template_url_service.h for more details.
 */
public class TemplateUrlService {

    /**
     * This listener will be notified when template url service is done loading.
     */
    public interface LoadListener {
        public abstract void onTemplateUrlServiceLoaded();
    }

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

    private final int mNativeTemplateUrlServiceAndroid;
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

    public int getSearchEngine() {
        ThreadUtils.assertOnUiThread();
        return nativeGetDefaultSearchProvider(mNativeTemplateUrlServiceAndroid);
    }

    public void setSearchEngine(int selectedIndex) {
        ThreadUtils.assertOnUiThread();
        nativeSetDefaultSearchProvider(mNativeTemplateUrlServiceAndroid, selectedIndex);
    }

    /**
     * Registers a listener for the TEMPLATE_URL_SERVICE_LOADED notification.
     */
    public void registerLoadListener(LoadListener listener) {
        ThreadUtils.assertOnUiThread();
        assert !mLoadListeners.hasObserver(listener);
        mLoadListeners.addObserver(listener);
    }

    /**
     * Unregisters a listener for the TEMPLATE_URL_SERVICE_LOADED notification.
     */
    public void unregisterLoadListener(LoadListener listener) {
        ThreadUtils.assertOnUiThread();
        assert (mLoadListeners.hasObserver(listener));
        mLoadListeners.removeObserver(listener);
    }

    private native int nativeInit();
    private native void nativeLoad(int nativeTemplateUrlServiceAndroid);
    private native boolean nativeIsLoaded(int nativeTemplateUrlServiceAndroid);
    private native int nativeGetTemplateUrlCount(int nativeTemplateUrlServiceAndroid);
    private native TemplateUrl nativeGetPrepopulatedTemplateUrlAt(
            int nativeTemplateUrlServiceAndroid, int i);
    private native void nativeSetDefaultSearchProvider(int nativeTemplateUrlServiceAndroid,
            int selectedIndex);
    private native int nativeGetDefaultSearchProvider(int nativeTemplateUrlServiceAndroid);
}
