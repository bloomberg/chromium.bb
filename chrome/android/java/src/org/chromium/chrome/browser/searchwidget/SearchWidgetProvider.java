// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.ActivityOptionsCompat;
import android.text.TextUtils;
import android.view.View;
import android.widget.RemoteViews;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.LoadListener;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.chrome.browser.util.IntentUtils;

/**
 * Widget that lets the user search using their default search engine.
 *
 * Because this is a BroadcastReceiver, it dies immediately after it runs.  A new one is created
 * for each new broadcast.
 *
 * This class avoids loading the native library because it can be triggered at regular intervals by
 * Android when it tells widgets that they need updates.
 */
public class SearchWidgetProvider extends AppWidgetProvider {
    /** Wraps up all things that a {@link SearchWidgetProvider} can request things from. */
    static class SearchWidgetProviderDelegate {
        private final Context mContext;
        private final AppWidgetManager mManager;

        public SearchWidgetProviderDelegate(Context context) {
            mContext = context == null ? ContextUtils.getApplicationContext() : context;
            mManager = AppWidgetManager.getInstance(mContext);
        }

        /** Returns the Context to pull resources from. */
        protected Context getContext() {
            return mContext;
        }

        /** See {@link ContextUtils#getAppSharedPreferences}. */
        protected SharedPreferences getSharedPreferences() {
            return ContextUtils.getAppSharedPreferences();
        }

        /** Returns IDs for all search widgets that exist. */
        protected int[] getAllSearchWidgetIds() {
            return mManager.getAppWidgetIds(
                    new ComponentName(getContext(), SearchWidgetProvider.class.getName()));
        }

        /** See {@link AppWidgetManager#updateAppWidget}. */
        protected void updateAppWidget(int id, RemoteViews views) {
            mManager.updateAppWidget(id, views);
        }
    }

    /** Monitors the TemplateUrlService for changes, updating the widget when necessary. */
    private static final class SearchWidgetTemplateUrlServiceObserver
            implements LoadListener, TemplateUrlServiceObserver {
        @Override
        public void onTemplateUrlServiceLoaded() {
            TemplateUrlService.getInstance().unregisterLoadListener(this);
            updateCachedEngineName();
        }

        @Override
        public void onTemplateURLServiceChanged() {
            updateCachedEngineName();
        }

        private void updateCachedEngineName() {
            assert LibraryLoader.isInitialized();

            // Getting an instance of the TemplateUrlService requires that the native library be
            // loaded, but the TemplateUrlService also itself needs to be initialized.
            TemplateUrlService service = TemplateUrlService.getInstance();
            assert service.isLoaded();
            SearchWidgetProvider.updateCachedEngineName(
                    service.getDefaultSearchEngineTemplateUrl().getShortName());
        }
    }

    static final String ACTION_START_TEXT_QUERY =
            "org.chromium.chrome.browser.searchwidget.START_TEXT_QUERY";
    static final String ACTION_START_VOICE_QUERY =
            "org.chromium.chrome.browser.searchwidget.START_VOICE_QUERY";
    static final String ACTION_UPDATE_ALL_WIDGETS =
            "org.chromium.chrome.browser.searchwidget.UPDATE_ALL_WIDGETS";

    static final String EXTRA_START_VOICE_SEARCH =
            "org.chromium.chrome.browser.searchwidget.START_VOICE_SEARCH";

    private static final String PREF_IS_VOICE_SEARCH_AVAILABLE =
            "org.chromium.chrome.browser.searchwidget.IS_VOICE_SEARCH_AVAILABLE";
    private static final String PREF_SEARCH_ENGINE_SHORTNAME =
            "org.chromium.chrome.browser.searchwidget.SEARCH_ENGINE_SHORTNAME";

    private static final String TAG = "searchwidget";
    private static final Object DELEGATE_LOCK = new Object();
    private static final Object OBSERVER_LOCK = new Object();

    private static SearchWidgetTemplateUrlServiceObserver sObserver;
    private static SearchWidgetProviderDelegate sDelegate;

    /**
     * Creates the singleton instance of the observer that will monitor for search engine changes.
     * The native library and the browser process must have been fully loaded before calling this.
     */
    public static void initialize() {
        ThreadUtils.assertOnUiThread();
        assert LibraryLoader.isInitialized();

        // Set up an observer to monitor for changes.
        synchronized (OBSERVER_LOCK) {
            if (sObserver != null) return;
            sObserver = new SearchWidgetTemplateUrlServiceObserver();

            TemplateUrlService service = TemplateUrlService.getInstance();
            service.registerLoadListener(sObserver);
            service.addObserver(sObserver);
            if (!service.isLoaded()) service.load();
        }
    }

    /** Nukes all cached information and forces all widgets to start with a blank slate. */
    public static void reset() {
        SharedPreferences.Editor editor = getDelegate().getSharedPreferences().edit();
        editor.remove(PREF_IS_VOICE_SEARCH_AVAILABLE);
        editor.remove(PREF_SEARCH_ENGINE_SHORTNAME);
        editor.apply();

        performUpdate(null);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (IntentHandler.isIntentChromeOrFirstParty(intent)) {
            handleAction(intent.getAction());
        } else {
            super.onReceive(context, intent);
        }
    }

    @Override
    public void onUpdate(Context context, AppWidgetManager manager, int[] ids) {
        performUpdate(ids);
    }

    /** Handles the intent actions to the widget. */
    @VisibleForTesting
    static void handleAction(String action) {
        if (ACTION_START_TEXT_QUERY.equals(action)) {
            startSearchActivity(false);
        } else if (ACTION_START_VOICE_QUERY.equals(action)) {
            startSearchActivity(true);
        } else if (ACTION_UPDATE_ALL_WIDGETS.equals(action)) {
            performUpdate(null);
        } else {
            assert false;
        }
    }

    private static void startSearchActivity(boolean startVoiceSearch) {
        Log.d(TAG, "Launching SearchActivity: VOICE=" + startVoiceSearch);
        Context context = getDelegate().getContext();

        // Launch the SearchActivity.
        Intent searchIntent = new Intent();
        searchIntent.setClass(context, SearchActivity.class);
        searchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        searchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        searchIntent.putExtra(EXTRA_START_VOICE_SEARCH, startVoiceSearch);

        Bundle optionsBundle =
                ActivityOptionsCompat.makeCustomAnimation(context, R.anim.activity_open_enter, 0)
                        .toBundle();
        IntentUtils.safeStartActivity(context, searchIntent, optionsBundle);
    }

    private static void performUpdate(int[] ids) {
        SearchWidgetProviderDelegate delegate = getDelegate();

        if (ids == null) ids = delegate.getAllSearchWidgetIds();
        if (ids.length == 0) return;

        SharedPreferences prefs = delegate.getSharedPreferences();
        boolean isVoiceSearchAvailable = getCachedVoiceSearchAvailability(prefs);
        String engineName = getCachedEngineName(prefs);

        for (int id : ids) {
            RemoteViews views = createWidgetViews(
                    delegate.getContext(), id, engineName, isVoiceSearchAvailable);
            delegate.updateAppWidget(id, views);
        }
    }

    private static RemoteViews createWidgetViews(
            Context context, int id, String engineName, boolean isVoiceSearchAvailable) {
        RemoteViews views =
                new RemoteViews(context.getPackageName(), R.layout.search_widget_template);

        // Clicking on the widget fires an Intent back at this BroadcastReceiver, allowing control
        // over how the Activity is animated when it starts up.
        Intent textIntent = createStartQueryIntent(context, ACTION_START_TEXT_QUERY, id);
        views.setOnClickPendingIntent(R.id.text_container,
                PendingIntent.getBroadcast(
                        context, 0, textIntent, PendingIntent.FLAG_UPDATE_CURRENT));

        // If voice search is available, clicking on the microphone triggers a voice query.
        if (isVoiceSearchAvailable) {
            Intent voiceIntent = createStartQueryIntent(context, ACTION_START_VOICE_QUERY, id);
            views.setOnClickPendingIntent(R.id.microphone_icon,
                    PendingIntent.getBroadcast(
                            context, 0, voiceIntent, PendingIntent.FLAG_UPDATE_CURRENT));
            views.setViewVisibility(R.id.microphone_icon, View.VISIBLE);
        } else {
            views.setViewVisibility(R.id.microphone_icon, View.GONE);
        }

        // Update what string is displayed by the widget.
        String text = TextUtils.isEmpty(engineName)
                ? context.getString(R.string.search_widget_default)
                : context.getString(R.string.search_with_product, engineName);
        views.setTextViewText(R.id.title, text);

        return views;
    }

    /** Caches whether or not a voice search is possible. */
    static void updateCachedVoiceSearchAvailability(boolean isVoiceSearchAvailable) {
        SharedPreferences prefs = getDelegate().getSharedPreferences();
        if (getCachedVoiceSearchAvailability(prefs) != isVoiceSearchAvailable) {
            prefs.edit().putBoolean(PREF_IS_VOICE_SEARCH_AVAILABLE, isVoiceSearchAvailable).apply();
            performUpdate(null);
        }
    }

    /**
     * Updates the name of the user's default search engine that is cached in SharedPreferences.
     * Caching it in SharedPreferences prevents us from having to load the native library and the
     * TemplateUrlService whenever the widget is updated.
     */
    static void updateCachedEngineName(String engineName) {
        SharedPreferences prefs = getDelegate().getSharedPreferences();
        if (!TextUtils.equals(getCachedEngineName(prefs), engineName)) {
            prefs.edit().putString(PREF_SEARCH_ENGINE_SHORTNAME, engineName).apply();
            performUpdate(null);
        }
    }

    /** Creates a trusted Intent that lets the user begin performing queries. */
    private static Intent createStartQueryIntent(Context context, String action, int widgetId) {
        Intent intent = new Intent(action, Uri.parse(String.valueOf(widgetId)));
        intent.setClass(context, SearchWidgetProvider.class);
        IntentHandler.addTrustedIntentExtras(intent);
        return intent;
    }

    /** Sets an {@link SearchWidgetProviderDelegate} to interact with. */
    @VisibleForTesting
    static void setDelegateForTest(SearchWidgetProviderDelegate delegate) {
        assert sDelegate == null;
        sDelegate = delegate;
    }

    private static boolean getCachedVoiceSearchAvailability(SharedPreferences prefs) {
        return prefs.getBoolean(PREF_IS_VOICE_SEARCH_AVAILABLE, true);
    }

    private static String getCachedEngineName(SharedPreferences prefs) {
        return prefs.getString(PREF_SEARCH_ENGINE_SHORTNAME, null);
    }

    private static SearchWidgetProviderDelegate getDelegate() {
        synchronized (DELEGATE_LOCK) {
            if (sDelegate == null) sDelegate = new SearchWidgetProviderDelegate(null);
        }
        return sDelegate;
    }
}
