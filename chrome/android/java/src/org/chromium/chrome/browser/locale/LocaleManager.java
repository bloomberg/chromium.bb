// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeFeatureList;

/**
 * Manager for some locale specific logics.
 */
public class LocaleManager {
    public static final String PREF_PROMO_SHOWN = "LocaleManager_PREF_PROMO_SHOWN";
    public static final String SPECIAL_LOCALE_ID = "US";

    private static LocaleManager sInstance;

    private SpecialLocaleHandler mLocaleHandler;

    /**
     * @return An instance of the {@link LocaleManager}. This should only be called on UI thread.
     */
    public static LocaleManager getInstance() {
        assert ThreadUtils.runningOnUiThread();
        if (sInstance == null) {
            sInstance = ((ChromeApplication) ContextUtils.getApplicationContext())
                    .createLocaleManager();
        }
        return sInstance;
    }

    /**
     * Starts recording metrics in deferred startup.
     */
    public void recordStartupMetrics() {}

    /**
     * @return Whether the Chrome instance is running in a special locale.
     */
    public boolean isSpecialLocaleEnabled() {
        // If there is a kill switch sent from the server, disable the feature.
        if (!ChromeFeatureList.isEnabled("SpecialLocaleWrapper")) {
            return false;
        }
        boolean inSpecialLocale = ChromeFeatureList.isEnabled("SpecialLocale");
        return isReallyInSpecialLocale(inSpecialLocale);
    }

    /**
     * @return The country id of the special locale.
     */
    public String getSpecialLocaleId() {
        return SPECIAL_LOCALE_ID;
    }

    /**
     * Adds local search engines for special locale.
     */
    public void addSpecialSearchEngines() {
        // TODO(ianwen): Let this method be called in ChromeActivity#finishNativeInitialization().
        if (!isSpecialLocaleEnabled()) return;
        getSpecialLocaleHandler().loadTemplateUrls();
    }

    /**
     * Overrides the default search engine to a different search engine we designate. This is a
     * no-op if the user has changed DSP settings before.
     */
    public void overrideDefaultSearchEngine() {
        // TODO(ianwen): Implement search engine auto switching.
        if (!isSpecialLocaleEnabled()) return;
        getSpecialLocaleHandler().overrideDefaultSearchProvider();
    }

    /**
     * Removes local search engines for special locale.
     */
    public void removeSpecialSearchEngines() {
        // TODO(ianwen): Let this method be called when device configuration changes.
        if (isSpecialLocaleEnabled()) return;
        getSpecialLocaleHandler().removeTemplateUrls();
    }

    /**
     * Shows a promotion dialog saying the default search engine will be set to Sogou. No-op if
     * device is not in special locale.
     *
     * @return Whether such dialog is needed.
     */
    public boolean showSearchEnginePromoIfNeeded(Context context) {
        if (!isSpecialLocaleEnabled()) return false;
        SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
        if (preferences.getBoolean(PREF_PROMO_SHOWN, false)) {
            return false;
        }

        new SearchEnginePromoDialog(context, this).show();

        preferences.edit().putBoolean(PREF_PROMO_SHOWN, true).apply();
        return true;
    }

    /**
     * Does some extra checking about whether the user is in special locale.
     * @param inSpecialLocale Whether the variation service thinks the client is in special locale.
     * @return The result after extra confirmation.
     */
    protected boolean isReallyInSpecialLocale(boolean inSpecialLocale) {
        return inSpecialLocale;
    }

    private SpecialLocaleHandler getSpecialLocaleHandler() {
        if (mLocaleHandler == null) mLocaleHandler = new SpecialLocaleHandler(getSpecialLocaleId());
        return mLocaleHandler;
    }
}
