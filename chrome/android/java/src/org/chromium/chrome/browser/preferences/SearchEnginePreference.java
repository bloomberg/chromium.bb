// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.content.res.Resources;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.LoadListener;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrl;

import java.util.List;

/**
 * Preference that allows the user to choose a search engine.
 */
public class SearchEnginePreference extends ChromeBaseListPreference implements LoadListener,
        OnPreferenceChangeListener {

    static final String PREF_SEARCH_ENGINE = "search_engine";

    /**
     * Constructor for inflating from XML.
     */
    public SearchEnginePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setPersistent(false);
        initEntries();

        setManagedPreferenceDelegate(new ManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return TemplateUrlService.getInstance().isSearchProviderManaged();
            }
        });
    }

    /**
     * @return The name of the search engine followed by the domain, e.g. "Google (google.co.uk)".
     */
    private static String getSearchEngineNameAndDomain(Resources res, TemplateUrl searchEngine) {
        String title = searchEngine.getShortName();
        if (!searchEngine.getKeyword().isEmpty()) {
            title = res.getString(R.string.search_engine_name_and_domain, title,
                    searchEngine.getKeyword());
        }
        return title;
    }

    private void initEntries() {
        TemplateUrlService templateUrlService = TemplateUrlService.getInstance();
        if (!templateUrlService.isLoaded()) {
            setEnabled(false);
            templateUrlService.registerLoadListener(this);
            templateUrlService.load();
            return;
        }

        List<TemplateUrl> searchEngines = templateUrlService.getLocalizedSearchEngines();
        int currentSearchEngineIndex = templateUrlService.getDefaultSearchEngineIndex();

        Resources resources = getContext().getResources();
        CharSequence[] entries = new CharSequence[searchEngines.size()];
        CharSequence[] entryValues = new CharSequence[searchEngines.size()];
        for (int i = 0; i < entries.length; i++) {
            TemplateUrl templateUrl = searchEngines.get(i);
            entries[i] = getSearchEngineNameAndDomain(resources, templateUrl);
            entryValues[i] = Integer.toString(templateUrl.getIndex());
        }

        setEntries(entries);
        setEntryValues(entryValues);
        setValueIndex(currentSearchEngineIndex);
        setOnPreferenceChangeListener(this);
    }

    @Override
    public CharSequence getSummary() {
        // Show the currently selected value as the summary.
        return getEntry();
    }

    // OnPreferenceChangeListener

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        int newSearchEngineIndex = Integer.parseInt((String) newValue);
        TemplateUrlService.getInstance().setSearchEngine(newSearchEngineIndex);
        return true;
    }

    // TemplateUrlService.LoadListener

    @Override
    public void onTemplateUrlServiceLoaded() {
        TemplateUrlService.getInstance().unregisterLoadListener(this);
        setEnabled(true);
        initEntries();
    }
}
