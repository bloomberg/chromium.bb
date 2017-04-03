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
import android.graphics.Rect;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.ActivityOptionsCompat;
import android.text.TextUtils;
import android.widget.RemoteViews;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.LoadListener;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrlServiceObserver;

/**
 * Widget that lets the user search using the default search engine in Chrome.
 * Because this is a BroadcastReceiver, it dies immediately after it runs.  A new one is created
 * for each new broadcast.
 */
public class SearchWidgetProvider extends AppWidgetProvider {
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
    }

    static final int INVALID_WIDGET_ID = -1;

    static final boolean ANIMATE_TRANSITION = false;

    private static final String ACTION_START_TEXT_QUERY =
            "org.chromium.chrome.browser.searchwidget.START_TEXT_QUERY";
    private static final String ACTION_START_VOICE_QUERY =
            "org.chromium.chrome.browser.searchwidget.START_VOICE_QUERY";
    private static final String ACTION_UPDATE_ALL_WIDGETS =
            "org.chromium.chrome.browser.searchwidget.UPDATE_ALL_WIDGETS";

    static final String EXTRA_START_VOICE_SEARCH =
            "org.chromium.chrome.browser.searchwidget.START_VOICE_SEARCH";
    private static final String EXTRA_WIDGET_ID =
            "org.chromium.chrome.browser.searchwidget.WIDGET_ID";

    private static final String PREF_LAUNCHING_WIDGET_ID =
            "org.chromium.chrome.browser.searchwidget.LAUNCHING_WIDGET_ID";
    private static final String PREF_SEARCH_ENGINE_SHORTNAME =
            "org.chromium.chrome.browser.searchwidget.SEARCH_ENGINE_SHORTNAME";
    static final String PREF_USE_HERB_TAB = "org.chromium.chrome.browser.searchwidget.USE_HERB_TAB";

    private static final String TAG = "searchwidget";
    private static final Object OBSERVER_LOCK = new Object();

    private static SearchWidgetTemplateUrlServiceObserver sObserver;
    private static String sCachedSearchEngineName;

    /**
     * Creates the singleton instance of the observer that will monitor for search engine changes.
     * The native library and the browser process must have been fully loaded before calling this,
     * after {@link AsyncInitializationActivity#finishNativeInitialization}.
     */
    public static void initialize() {
        ThreadUtils.assertOnUiThread();
        assert LibraryLoader.isInitialized();

        // Set up an observer to monitor for changes.
        synchronized (OBSERVER_LOCK) {
            if (sObserver != null) return;
            sObserver = new SearchWidgetTemplateUrlServiceObserver();

            TemplateUrlService service = TemplateUrlService.getInstance();
            service.addObserver(sObserver);
            if (!service.isLoaded()) {
                service.registerLoadListener(sObserver);
                service.load();
            }
        }
        updateCachedEngineName();
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (IntentHandler.isIntentChromeOrFirstParty(intent)) {
            if (ACTION_START_TEXT_QUERY.equals(intent.getAction())) {
                startSearchActivity(context, intent, false);
                return;
            } else if (ACTION_START_VOICE_QUERY.equals(intent.getAction())) {
                startSearchActivity(context, intent, true);
                return;
            }
        } else if (ACTION_UPDATE_ALL_WIDGETS.equals(intent.getAction())) {
            performUpdate(context);
            return;
        }
        super.onReceive(context, intent);
    }

    private void startSearchActivity(Context context, Intent intent, boolean startVoiceSearch) {
        int widgetId = getLaunchingWidgetIdFromIntent(intent);
        SearchWidgetProvider.setLaunchingWidgetId(widgetId);
        Log.d(TAG, "Launching SearchActivity: ID=" + widgetId + " VOICE=" + startVoiceSearch);

        // Launch the SearchActivity.
        Intent searchIntent = new Intent();
        searchIntent.setClassName(context, SearchActivity.class.getName());
        searchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        searchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        searchIntent.putExtra(EXTRA_START_VOICE_SEARCH, startVoiceSearch);

        Bundle optionsBundle;
        if (ANIMATE_TRANSITION) {
            // Pass the widget's bounds along to allow moving the box into place.
            Rect rect = intent.getSourceBounds();
            if (rect != null) searchIntent.setSourceBounds(rect);
            optionsBundle = ActivityOptionsCompat.makeCustomAnimation(context, 0, 0).toBundle();
        } else {
            optionsBundle = ActivityOptionsCompat
                                    .makeCustomAnimation(context, R.anim.activity_open_enter, 0)
                                    .toBundle();
        }
        context.startActivity(searchIntent, optionsBundle);
    }

    @Override
    public void onUpdate(Context context, AppWidgetManager manager, int[] ids) {
        performUpdate(context, ids);
        super.onUpdate(context, manager, ids);
    }

    private void performUpdate(Context context) {
        AppWidgetManager manager = AppWidgetManager.getInstance(context);
        performUpdate(context, getAllSearchWidgetIds(manager));
    }

    private void performUpdate(Context context, int[] ids) {
        for (int id : ids) updateWidget(context, id);
    }

    private void updateWidget(Context context, int id) {
        int launchingWidgetId = ANIMATE_TRANSITION ? getLaunchingWidgetId() : INVALID_WIDGET_ID;

        AppWidgetManager manager = AppWidgetManager.getInstance(context);
        RemoteViews views = new RemoteViews(context.getPackageName(),
                id == launchingWidgetId ? R.layout.search_widget_template_transparent
                                        : R.layout.search_widget_template);

        // Clicking on the widget fires an Intent back at this BroadcastReceiver, allowing control
        // over how the Activity is animated when it starts up.
        Intent textIntent = createStartQueryIntent(context, ACTION_START_TEXT_QUERY, id);
        views.setOnClickPendingIntent(R.id.text_container,
                PendingIntent.getBroadcast(
                        context, 0, textIntent, PendingIntent.FLAG_UPDATE_CURRENT));

        Intent voiceIntent = createStartQueryIntent(context, ACTION_START_VOICE_QUERY, id);
        views.setOnClickPendingIntent(R.id.microphone_icon,
                PendingIntent.getBroadcast(
                        context, 0, voiceIntent, PendingIntent.FLAG_UPDATE_CURRENT));

        // Update what string is displayed by the widget.
        String engineName = getCachedEngineName();
        String text = TextUtils.isEmpty(engineName)
                ? context.getString(R.string.search_widget_default)
                : context.getString(R.string.search_with_product, engineName);
        views.setTextViewText(R.id.title, text);

        manager.updateAppWidget(id, views);
    }

    /** Force all widgets to update their state. */
    static void updateAllWidgets() {
        Intent intent = new Intent(ACTION_UPDATE_ALL_WIDGETS);
        intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
        ContextUtils.getApplicationContext().sendBroadcast(intent);
    }

    /** Updates the name of the user's default search engine that is cached in SharedPreferences. */
    static void updateCachedEngineName() {
        assert LibraryLoader.isInitialized();

        // Getting an instance of the TemplateUrlService requires that the native library be
        // loaded, but the TemplateUrlService itself needs to be initialized.
        TemplateUrlService service = TemplateUrlService.getInstance();
        if (!service.isLoaded()) return;

        String engineName = service.getDefaultSearchEngineTemplateUrl().getShortName();
        if (!TextUtils.equals(sCachedSearchEngineName, engineName)) {
            sCachedSearchEngineName = engineName;

            SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();
            editor.putString(PREF_SEARCH_ENGINE_SHORTNAME, engineName);
            editor.apply();

            updateAllWidgets();
        }
    }

    /**
     * Returns the cached name of the user's default search engine.  Caching it in SharedPreferences
     * prevents us from having to load the native library and the TemplateUrlService.
     *
     * @return The cached name of the user's default search engine.
     */
    private static String getCachedEngineName() {
        if (sCachedSearchEngineName != null) return sCachedSearchEngineName;

        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        sCachedSearchEngineName = prefs.getString(PREF_SEARCH_ENGINE_SHORTNAME, null);
        return sCachedSearchEngineName;
    }

    /** Get the IDs of all of Chrome's search widgets. */
    private static int[] getAllSearchWidgetIds(AppWidgetManager manager) {
        return manager.getAppWidgetIds(new ComponentName(
                ContextUtils.getApplicationContext(), SearchWidgetProvider.class.getName()));
    }

    private int getLaunchingWidgetId() {
        int launchingWidgetId = INVALID_WIDGET_ID;

        // If the SearchActivity isn't in the foreground, the user must have exited it.
        if (ApplicationStatus.getLastTrackedFocusedActivity() instanceof SearchActivity) {
            launchingWidgetId = ContextUtils.getAppSharedPreferences().getInt(
                    PREF_LAUNCHING_WIDGET_ID, INVALID_WIDGET_ID);
        }

        return launchingWidgetId;
    }

    /** Cache the ID of the widget that was used to launch the SearchActivity. */
    static void setLaunchingWidgetId(int id) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putInt(PREF_LAUNCHING_WIDGET_ID, id);
        editor.apply();
    }

    /** Parse out which widget launched the Activity from the given Intent. */
    private static int getLaunchingWidgetIdFromIntent(Intent intent) {
        String data = intent.getData() == null ? null : intent.getData().toString();
        if (data == null) return INVALID_WIDGET_ID;

        try {
            return Integer.parseInt(data);
        } catch (NumberFormatException e) {
            return INVALID_WIDGET_ID;
        }
    }

    /** Creates a trusted Intent that lets the user begin performing queries. */
    private Intent createStartQueryIntent(Context context, String action, int widgetId) {
        Intent intent = new Intent(action, Uri.parse(String.valueOf(widgetId)));
        intent.setClass(context, getClass());
        IntentHandler.addTrustedIntentExtras(intent);
        return intent;
    }
}
