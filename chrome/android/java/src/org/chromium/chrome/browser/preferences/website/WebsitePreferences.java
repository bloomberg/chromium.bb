// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceFragment;
import android.preference.PreferenceGroup;
import android.preference.PreferenceScreen;
import android.support.v4.view.MenuItemCompat;
import android.support.v7.widget.SearchView;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.inputmethod.EditorInfo;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeBaseCheckBoxPreference;
import org.chromium.chrome.browser.preferences.ChromeBasePreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.ExpandablePreferenceGroup;
import org.chromium.chrome.browser.preferences.LocationSettings;
import org.chromium.chrome.browser.preferences.ManagedPreferenceDelegate;
import org.chromium.chrome.browser.preferences.ManagedPreferencesUtils;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Shows a list of sites with their associated HTML5 settings. When the
 * users selects a site, a SingleWebsitePreferences fragment is launched to
 * allow the user to modify the settings.
 */
public class WebsitePreferences extends PreferenceFragment
        implements OnPreferenceChangeListener, OnPreferenceClickListener,
                   AddExceptionPreference.SiteAddedCallback {
    // The key to use to pass which category this preference should display,
    // e.g. Location/Popups/All sites (if blank).
    public static final String EXTRA_CATEGORY = "category";
    public static final String EXTRA_TITLE = "title";

    // The view to show when the list is empty.
    private TextView mEmptyView;
    // The view for searching the list of items.
    private SearchView mSearchView;
    // What category the list is filtered by (e.g. show all, location, storage,
    // etc). For full list see WebsiteSettingsCategoryFilter.
    private String mCategoryFilter = "";
    // The filter helper object.
    private WebsiteSettingsCategoryFilter mFilter = null;
    // If not blank, represents a substring to use to search for site names.
    private String mSearch = "";
    // Whether to group by allowed/blocked list.
    private boolean mGroupByAllowBlock = false;
    // Whether the Blocked list should be shown expanded.
    private boolean mBlockListExpanded = false;
    // Whether the Allowed list should be shown expanded.
    private boolean mAllowListExpanded = true;
    // Whether this is the first time this screen is shown.
    private boolean mIsInitialRun = true;
    // The number of sites that are on the Allowed list.
    private int mAllowedSiteCount = 0;

    // Keys for individual preferences.
    public static final String READ_WRITE_TOGGLE_KEY = "read_write_toggle";
    public static final String THIRD_PARTY_COOKIES_TOGGLE_KEY = "third_party_cookies";
    private static final String ADD_EXCEPTION_KEY = "add_exception";
    // Keys for Allowed/Blocked preference groups/headers.
    private static final String ALLOWED_GROUP = "allowed_group";
    private static final String BLOCKED_GROUP = "blocked_group";

    private void getInfoForOrigins() {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(new ResultsPopulator());
        fetcher.fetchPreferencesWithFilter(mCategoryFilter);
    }

    private void displayEmptyScreenMessage() {
        if (mEmptyView != null) {
            mEmptyView.setText(R.string.no_saved_website_settings);
        }
    }

    private class ResultsPopulator implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        @Override
        public void onWebsitePermissionsAvailable(
                Map<String, Set<Website>> sitesByOrigin, Map<String, Set<Website>> sitesByHost) {
            // This method may be called after the activity has been destroyed.
            // In that case, bail out.
            if (getActivity() == null) return;

            // First we scan origins to get settings from there.
            List<WebsitePreference> websites = new ArrayList<>();
            Set<Website> displayedSites = new HashSet<>();
            for (Map.Entry<String, Set<Website>> element : sitesByOrigin.entrySet()) {
                for (Website site : element.getValue()) {
                    if (mSearch.isEmpty() || site.getTitle().contains(mSearch)) {
                        websites.add(new WebsitePreference(getActivity(), site, mCategoryFilter));
                        displayedSites.add(site);
                    }
                }
            }
            // Next we add sites that are only accessible by host name.
            for (Map.Entry<String, Set<Website>> element : sitesByHost.entrySet()) {
                for (Website site : element.getValue()) {
                    if (!displayedSites.contains(site)) {
                        if (mSearch.isEmpty() || site.getTitle().contains(mSearch)) {
                            websites.add(new WebsitePreference(
                                    getActivity(), site, mCategoryFilter));
                            displayedSites.add(site);
                        }
                    }
                }
            }

            resetList();
            Collections.sort(websites);
            mAllowedSiteCount = 0;
            int blocked = 0;
            if (websites.size() > 0) {
                if (!mGroupByAllowBlock) {
                    // We're not grouping sites into Allowed/Blocked lists, so show all in order
                    // (will be alphabetical).
                    for (WebsitePreference website : websites) {
                        getPreferenceScreen().addPreference(website);
                    }
                } else {
                    // Group sites into Allowed/Blocked lists.
                    PreferenceGroup allowedGroup =
                            (PreferenceGroup) getPreferenceScreen().findPreference(
                                    ALLOWED_GROUP);
                    PreferenceGroup blockedGroup =
                            (PreferenceGroup) getPreferenceScreen().findPreference(
                                    BLOCKED_GROUP);

                    for (WebsitePreference website : websites) {
                        if (isOnBlockList(website)) {
                            blockedGroup.addPreference(website);
                            blocked += 1;
                        } else {
                            allowedGroup.addPreference(website);
                            mAllowedSiteCount += 1;
                        }
                    }

                    // The default, when the two lists are shown for the first time, is for the
                    // Blocked list to be collapsed and Allowed expanded -- because the data in
                    // the Allowed list is normally more useful than the data in the Blocked
                    // list. A collapsed initial Blocked list works well *except* when there's
                    // nothing in the Allowed list because then there's only Blocked items to
                    // show and it doesn't make sense for those items to be hidden. So, in that
                    // case (and only when the list is shown for the first time) do we ignore
                    // the collapsed directive. The user can still collapse and expand the
                    // Blocked list at will.
                    if (mIsInitialRun) {
                        if (allowedGroup.getPreferenceCount() == 0) mBlockListExpanded = true;
                        mIsInitialRun = false;
                    }

                    if (!mBlockListExpanded) {
                        blockedGroup.removeAll();
                    }

                    if (!mAllowListExpanded) {
                        allowedGroup.removeAll();
                    }
                }

                updateBlockedHeader(blocked);
                ChromeSwitchPreference globalToggle = (ChromeSwitchPreference)
                        getPreferenceScreen().findPreference(READ_WRITE_TOGGLE_KEY);
                updateAllowedHeader(mAllowedSiteCount,
                                    (globalToggle != null ? globalToggle.isChecked() : true));
            } else {
                displayEmptyScreenMessage();
                updateBlockedHeader(0);
                updateAllowedHeader(0, true);
            }
        }
    }

    /**
     * Returns whether a website is on the Blocked list for the category currently showing (if any).
     * @param website The website to check.
     */
    private boolean isOnBlockList(WebsitePreference website) {
        if (mFilter.showCookiesSites(mCategoryFilter)) {
            return website.site().getCookiePermission() == ContentSetting.BLOCK;
        } else if (mFilter.showCameraMicSites(mCategoryFilter)) {
            return website.site().getVoiceCapturePermission() == ContentSetting.BLOCK
                    || website.site().getVideoCapturePermission() == ContentSetting.BLOCK;
        } else if (mFilter.showFullscreenSites(mCategoryFilter)) {
            return website.site().getFullscreenPermission() == ContentSetting.ASK;
        } else if (mFilter.showGeolocationSites(mCategoryFilter)) {
            return website.site().getGeolocationPermission() == ContentSetting.BLOCK;
        } else if (mFilter.showJavaScriptSites(mCategoryFilter)) {
            return website.site().getJavaScriptPermission() == ContentSetting.BLOCK;
        } else if (mFilter.showPopupSites(mCategoryFilter)) {
            return website.site().getPopupPermission() == ContentSetting.BLOCK;
        } else if (mFilter.showPushNotificationsSites(mCategoryFilter)) {
            return website.site().getPushNotificationPermission() == ContentSetting.BLOCK;
        }

        return false;
    }

    /**
     * Update the Category Header for the Allowed list.
     * @param numAllowed The number of sites that are on the Allowed list
     * @param toggleValue The value the global toggle will have once precessing ends.
     */
    private void updateAllowedHeader(int numAllowed, boolean toggleValue) {
        ExpandablePreferenceGroup allowedGroup =
                (ExpandablePreferenceGroup) getPreferenceScreen().findPreference(ALLOWED_GROUP);
        if (numAllowed == 0) {
            if (allowedGroup != null) getPreferenceScreen().removePreference(allowedGroup);
            return;
        }
        if (!mGroupByAllowBlock) return;

        // When the toggle is set to Blocked, the Allowed list header should read 'Exceptions', not
        // 'Allowed' (because it shows exceptions from the rule).
        int resourceId = toggleValue
                ? R.string.website_settings_allowed_group_heading
                : R.string.website_settings_exceptions_group_heading;

        // Set the title and arrow icons for the header.
        allowedGroup.setGroupTitle(resourceId, numAllowed);
        TintedDrawable icon = TintedDrawable.constructTintedDrawable(getResources(),
                mAllowListExpanded ? R.drawable.ic_expand : R.drawable.ic_collapse);
        allowedGroup.setIcon(icon);
    }

    private void updateBlockedHeader(int numBlocked) {
        ExpandablePreferenceGroup blockedGroup =
                (ExpandablePreferenceGroup) getPreferenceScreen().findPreference(BLOCKED_GROUP);
        if (numBlocked == 0) {
            if (blockedGroup != null) getPreferenceScreen().removePreference(blockedGroup);
            return;
        }
        if (!mGroupByAllowBlock) return;

        // Set the title and arrow icons for the header.
        blockedGroup.setGroupTitle(R.string.website_settings_blocked_group_heading, numBlocked);
        TintedDrawable icon = TintedDrawable.constructTintedDrawable(getResources(),
                mBlockListExpanded ? R.drawable.ic_expand : R.drawable.ic_collapse);
        blockedGroup.setIcon(icon);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        addPreferencesFromResource(R.xml.website_preferences);
        ListView listView = (ListView) getView().findViewById(android.R.id.list);
        mEmptyView = (TextView) getView().findViewById(android.R.id.empty);
        listView.setEmptyView(mEmptyView);
        listView.setDivider(null);

        // Read which category, if any, we should be showing.
        if (getArguments() != null) {
            mCategoryFilter = getArguments().getString(EXTRA_CATEGORY, "");
            String title = getArguments().getString(EXTRA_TITLE);
            if (title != null) getActivity().setTitle(title);
        }

        mFilter = new WebsiteSettingsCategoryFilter();

        configureGlobalToggles();

        setHasOptionsMenu(true);

        super.onActivityCreated(savedInstanceState);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        inflater.inflate(R.menu.website_preferences_menu, menu);

        MenuItem searchItem = menu.findItem(R.id.search);
        mSearchView = (SearchView) MenuItemCompat.getActionView(searchItem);
        mSearchView.setImeOptions(EditorInfo.IME_FLAG_NO_FULLSCREEN);

        SearchView.OnQueryTextListener queryTextListener =
                new SearchView.OnQueryTextListener() {
                    @Override
                    public boolean onQueryTextSubmit(String query) {
                        return true;
                    }

                    @Override
                    public boolean onQueryTextChange(String query) {
                        if (query.equals(mSearch)) return true;

                        mSearch = query;
                        getInfoForOrigins();
                        return true;
                    }
                };
        mSearchView.setOnQueryTextListener(queryTextListener);
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen screen, Preference preference) {
        if (isCategoryManaged()) {
            showManagedToast();
            return false;
        }

        if (!mSearch.isEmpty()) {
            // Clear out any lingering searches, so that the full list is shown
            // when coming back to this page.
            mSearch = "";
            mSearchView.setQuery("", false);
        }

        if (preference instanceof WebsitePreference) {
            WebsitePreference website = (WebsitePreference) preference;
            website.setFragment(SingleWebsitePreferences.class.getName());
            website.putSiteIntoExtras(SingleWebsitePreferences.EXTRA_SITE);
        }

        return super.onPreferenceTreeClick(screen, preference);
    }

    // OnPreferenceChangeListener:

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (READ_WRITE_TOGGLE_KEY.equals(preference.getKey())) {
            if (isCategoryManaged()) return false;

            if (mFilter.showGeolocationSites(mCategoryFilter)) {
                PrefServiceBridge.getInstance().setAllowLocationEnabled((boolean) newValue);
            } else if (mFilter.showCookiesSites(mCategoryFilter)) {
                PrefServiceBridge.getInstance().setAllowCookiesEnabled((boolean) newValue);
                updateThirdPartyCookiesCheckBox();
            } else if (mFilter.showCameraMicSites(mCategoryFilter)) {
                PrefServiceBridge.getInstance().setCameraMicEnabled((boolean) newValue);
            } else if (mFilter.showFullscreenSites(mCategoryFilter)) {
                PrefServiceBridge.getInstance().setFullscreenAllowed((boolean) newValue);
            } else if (mFilter.showJavaScriptSites(mCategoryFilter)) {
                PrefServiceBridge.getInstance().setJavaScriptEnabled((boolean) newValue);
                if ((boolean) newValue) {
                    Preference addException = getPreferenceScreen().findPreference(
                            ADD_EXCEPTION_KEY);
                    getPreferenceScreen().removePreference(addException);
                } else {
                    getPreferenceScreen().addPreference(
                            new AddExceptionPreference(getActivity(), ADD_EXCEPTION_KEY, this));
                }
            } else if (mFilter.showPopupSites(mCategoryFilter)) {
                PrefServiceBridge.getInstance().setAllowPopupsEnabled((boolean) newValue);
            } else if (mFilter.showPushNotificationsSites(mCategoryFilter)) {
                PrefServiceBridge.getInstance().setPushNotificationsEnabled((boolean) newValue);
            }

            ChromeSwitchPreference globalToggle = (ChromeSwitchPreference)
                    getPreferenceScreen().findPreference(READ_WRITE_TOGGLE_KEY);
            updateAllowedHeader(mAllowedSiteCount, !globalToggle.isChecked());
        } else if (THIRD_PARTY_COOKIES_TOGGLE_KEY.equals(preference.getKey())) {
            PrefServiceBridge.getInstance().setBlockThirdPartyCookiesEnabled(!((boolean) newValue));
        }
        return true;
    }

    // OnPreferenceClickListener:

    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (ALLOWED_GROUP.equals(preference.getKey()))  {
            mAllowListExpanded = !mAllowListExpanded;
        } else {
            mBlockListExpanded = !mBlockListExpanded;
        }
        getInfoForOrigins();
        return true;
    }

    @Override
    public void onResume() {
        super.onResume();

        getInfoForOrigins();
    }

    @Override
    public void onSiteAdded() {
        getInfoForOrigins();
    }

    /*
     * Returns whether the current category is managed either by enterprise policy or by the
     * custodian of a supervised account.
     */
    private boolean isCategoryManaged() {
        PrefServiceBridge prefs = PrefServiceBridge.getInstance();
        if (mFilter.showCameraMicSites(mCategoryFilter)) return !prefs.isCameraMicUserModifiable();
        if (mFilter.showCookiesSites(mCategoryFilter)) return prefs.isAcceptCookiesManaged();
        if (mFilter.showFullscreenSites(mCategoryFilter)) return prefs.isFullscreenManaged();
        if (mFilter.showGeolocationSites(mCategoryFilter)) {
            return !prefs.isAllowLocationUserModifiable();
        }
        if (mFilter.showJavaScriptSites(mCategoryFilter)) return prefs.javaScriptManaged();
        if (mFilter.showPopupSites(mCategoryFilter)) return prefs.isPopupsManaged();
        return false;
    }

    /*
     * Returns whether the current category is managed by the custodian (e.g. parent, not an
     * enterprise admin) of the account if the account is supervised.
     */
    private boolean isCategoryManagedByCustodian() {
        PrefServiceBridge prefs = PrefServiceBridge.getInstance();
        if (mFilter.showGeolocationSites(mCategoryFilter)) {
            return prefs.isAllowLocationManagedByCustodian();
        }
        if (mFilter.showCameraMicSites(mCategoryFilter)) {
            return prefs.isCameraMicManagedByCustodian();
        }
        return false;
    }

    /**
     * Reset the preference screen an initialize it again.
     */
    private void resetList() {
        // This will remove the combo box at the top and all the sites listed below it.
        getPreferenceScreen().removeAll();
        // And this will add the filter preference back (combo box).
        addPreferencesFromResource(R.xml.website_preferences);

        configureGlobalToggles();

        if (mFilter.showJavaScriptSites(mCategoryFilter)
                && !PrefServiceBridge.getInstance().javaScriptEnabled()) {
            getPreferenceScreen().addPreference(
                    new AddExceptionPreference(getActivity(), ADD_EXCEPTION_KEY, this));
        }
    }

    private void configureGlobalToggles() {
        // Only some have a global toggle at the top.
        ChromeSwitchPreference globalToggle = (ChromeSwitchPreference)
                getPreferenceScreen().findPreference(READ_WRITE_TOGGLE_KEY);

        Preference thirdPartyCookies = getPreferenceScreen().findPreference(
                THIRD_PARTY_COOKIES_TOGGLE_KEY);

        // Configure/hide the third-party cookie toggle, as needed.
        if (mFilter.showCookiesSites(mCategoryFilter)) {
            thirdPartyCookies.setOnPreferenceChangeListener(this);
            updateThirdPartyCookiesCheckBox();
        } else {
            getPreferenceScreen().removePreference(thirdPartyCookies);
        }

        if (mFilter.showAllSites(mCategoryFilter)
                    || mFilter.showStorageSites(mCategoryFilter)) {
            getPreferenceScreen().removePreference(globalToggle);
            getPreferenceScreen().removePreference(
                    getPreferenceScreen().findPreference(ALLOWED_GROUP));
            getPreferenceScreen().removePreference(
                    getPreferenceScreen().findPreference(BLOCKED_GROUP));
        } else {
            // When this menu opens, make sure the Blocked list is collapsed.
            if (!mGroupByAllowBlock) {
                mBlockListExpanded = false;
                mAllowListExpanded = true;
            }
            mGroupByAllowBlock = true;
            PreferenceGroup allowedGroup =
                    (PreferenceGroup) getPreferenceScreen().findPreference(
                            ALLOWED_GROUP);
            PreferenceGroup blockedGroup =
                    (PreferenceGroup) getPreferenceScreen().findPreference(
                            BLOCKED_GROUP);
            allowedGroup.setOnPreferenceClickListener(this);
            blockedGroup.setOnPreferenceClickListener(this);

            // Determine what toggle to use and what it should display.
            int type = mFilter.toContentSettingsType(mCategoryFilter);
            Website.PermissionDataEntry entry =
                    Website.PermissionDataEntry.getPermissionDataEntry(type);
            if (mFilter.showGeolocationSites(mCategoryFilter)
                    && !isCategoryManaged()
                    && !LocationSettings.getInstance().isSystemLocationSettingEnabled()) {
                getPreferenceScreen().removePreference(globalToggle);

                // Show the link to system settings since system location is disabled.
                ChromeBasePreference locationMessage =
                        new ChromeBasePreference(getActivity(), null);
                int color = getResources().getColor(R.color.pref_accent_color);
                ForegroundColorSpan linkSpan = new ForegroundColorSpan(color);
                final String message = getString(R.string.android_location_off);
                final SpannableString messageWithLink =
                        SpanApplier.applySpans(
                                message, new SpanInfo("<link>", "</link>", linkSpan));
                locationMessage.setTitle(messageWithLink);
                locationMessage.setIntent(
                        LocationSettings.getInstance().getSystemLocationSettingsIntent());
                getPreferenceScreen().addPreference(locationMessage);
            } else {
                globalToggle.setOnPreferenceChangeListener(this);
                globalToggle.setTitle(entry.titleResourceId);
                globalToggle.setSummaryOn(entry.getEnabledSummaryResourceId());
                globalToggle.setSummaryOff(entry.getDisabledSummaryResourceId());
                if (isCategoryManaged() && !isCategoryManagedByCustodian()) {
                    globalToggle.setIcon(R.drawable.controlled_setting_mandatory);
                }
                if (mFilter.showGeolocationSites(mCategoryFilter)) {
                    globalToggle.setChecked(
                            LocationSettings.getInstance().isChromeLocationSettingEnabled());
                } else if (mFilter.showCameraMicSites(mCategoryFilter)) {
                    globalToggle.setChecked(PrefServiceBridge.getInstance().isCameraMicEnabled());
                } else if (mFilter.showCookiesSites(mCategoryFilter)) {
                    globalToggle.setChecked(
                            PrefServiceBridge.getInstance().isAcceptCookiesEnabled());
                } else if (mFilter.showFullscreenSites(mCategoryFilter)) {
                    globalToggle.setChecked(
                            PrefServiceBridge.getInstance().isFullscreenAllowed());
                } else if (mFilter.showJavaScriptSites(mCategoryFilter)) {
                    globalToggle.setChecked(PrefServiceBridge.getInstance().javaScriptEnabled());
                } else if (mFilter.showPopupSites(mCategoryFilter)) {
                    globalToggle.setChecked(PrefServiceBridge.getInstance().popupsEnabled());
                } else if (mFilter.showPushNotificationsSites(mCategoryFilter)) {
                    globalToggle.setChecked(
                            PrefServiceBridge.getInstance().isPushNotificationsEnabled());
                }
            }
        }
    }

    private void updateThirdPartyCookiesCheckBox() {
        ChromeBaseCheckBoxPreference thirdPartyCookiesPref = (ChromeBaseCheckBoxPreference)
                getPreferenceScreen().findPreference(THIRD_PARTY_COOKIES_TOGGLE_KEY);
        thirdPartyCookiesPref.setEnabled(PrefServiceBridge.getInstance().isAcceptCookiesEnabled());
        thirdPartyCookiesPref.setManagedPreferenceDelegate(new ManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return PrefServiceBridge.getInstance().isBlockThirdPartyCookiesManaged();
            }
        });
    }

    private void showManagedToast() {
        if (isCategoryManagedByCustodian()) {
            ManagedPreferencesUtils.showManagedByParentToast(getActivity());
        } else {
            ManagedPreferencesUtils.showManagedByAdministratorToast(getActivity());
        }
    }
}
