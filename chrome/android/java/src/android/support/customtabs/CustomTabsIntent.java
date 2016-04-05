// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package android.support.customtabs;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.view.View;
import android.widget.RemoteViews;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;

/**
 * Constants and utilities that will be used for low level control on customizing the UI and
 * functionality of a tab.
 */
public class CustomTabsIntent {

    /**
     * Extra used to match the session. This has to be included in the intent to open in
     * a custom tab. This is the same IBinder that gets passed to ICustomTabsService#newSession.
     * Null if there is no need to match any service side sessions with the intent.
     */
    public static final String EXTRA_SESSION = "android.support.customtabs.extra.SESSION";

    /**
     * Extra that changes the background color for the toolbar. colorRes is an int that specifies a
     * {@link Color}, not a resource id.
     */
    public static final String EXTRA_TOOLBAR_COLOR =
            "android.support.customtabs.extra.TOOLBAR_COLOR";

    /**
     * Boolean extra that enables the url bar to hide as the user scrolls down the page
     */
    public static final String EXTRA_ENABLE_URLBAR_HIDING =
            "android.support.customtabs.extra.ENABLE_URLBAR_HIDING";

    /**
     * Bundle used for adding custom action buttons to the custom tab. The client should provide a
     * unique id, an icon {@link Bitmap} and a {@link PendingIntent} for each button.
     * <p>
     * The data associated with this extra can be either a bundle or an {@link ArrayList} of
     * bundles. If an {@link ArrayList} is given, only the bundle with the id of
     * {@link #TOOLBAR_ACTION_BUTTON_ID} will be displayed on toolbar; the rest will be put on a
     * bottom bar.
     */
    public static final String EXTRA_ACTION_BUTTON_BUNDLE =
            "android.support.customtabs.extra.ACTION_BUTTON_BUNDLE";

    /**
     * List<Bundle> used for adding items to the top and bottom toolbars. The client should
     * provide an ID, a description, an icon {@link Bitmap} for each item. They may also provide a
     * {@link PendingIntent} if the item is a button.
     */
    public static final String EXTRA_TOOLBAR_ITEMS =
            "android.support.customtabs.extra.TOOLBAR_ITEMS";

    /**
     * Key that specifies the {@link Bitmap} to be used as the image source for the action button.
     */
    public static final String KEY_ICON = "android.support.customtabs.customaction.ICON";

    /**
     * Key that specifies the content description for the custom action button.
     */
    public static final String KEY_DESCRIPTION =
            "android.support.customtabs.customaction.DESCRIPTION";

    /**
     * Key that specifies the PendingIntent to launch when the action button or menu item was
     * clicked. The custom tab will be calling {@link PendingIntent#send()} on clicks after adding
     * the url as data. The client app can call {@link Intent#getDataString()} to get the url.
     */
    public static final String KEY_PENDING_INTENT =
            "android.support.customtabs.customaction.PENDING_INTENT";

    /**
     * Use an {@code ArrayList<Bundle>} for specifying menu related params. There should be a
     * separate {@link Bundle} for each custom menu item.
     */
    public static final String EXTRA_MENU_ITEMS = "android.support.customtabs.extra.MENU_ITEMS";

    /**
     * Key for specifying the title of a menu item.
     */
    public static final String KEY_MENU_ITEM_TITLE =
            "android.support.customtabs.customaction.MENU_ITEM_TITLE";

    /**
     * Bundle constructed out of {@link ActivityOptions} that will be running when the
     * {@link Activity} that holds the custom tab gets finished. A similar ActivityOptions
     * for creation should be constructed and given to the startActivity() call that
     * launches the custom tab.
     */
    public static final String EXTRA_EXIT_ANIMATION_BUNDLE =
            "android.support.customtabs.extra.EXIT_ANIMATION_BUNDLE";

    /**
     * Extra bitmap that specifies the icon of the back button on the toolbar. If the client chooses
     * not to customize it, a default close button will be used.
     */
    public static final String EXTRA_CLOSE_BUTTON_ICON =
            "android.support.customtabs.extra.CLOSE_BUTTON_ICON";

    /**
     * Extra int that specifies state for showing the page title. Default is showing only the domain
     * and no information about the title.
     */
    public static final String EXTRA_TITLE_VISIBILITY_STATE =
            "android.support.customtabs.extra.TITLE_VISIBILITY";

    /**
     * Don't show any title. Shows only the domain.
     */
    public static final int NO_TITLE = 0;

    /**
     * Shows the page title and the domain.
     */
    public static final int SHOW_PAGE_TITLE = 1;

    /**
     * Extra boolean that specifies whether the custom action buttons should be tinted. Default is
     * false and the action buttons will not be tinted. If true, all buttons will be tinted,
     * regardless of their positions.
     */
    public static final String EXTRA_TINT_ACTION_BUTTON =
            "android.support.customtabs.extra.TINT_ACTION_BUTTON";

    /**
     * Boolean that specifies whether a default share button will be shown in the menu.
     */
    public static final String EXTRA_DEFAULT_SHARE_MENU_ITEM =
            "android.support.customtabs.extra.SHARE_MENU_ITEM";

    /**
     * Key that specifies the unique id for an action button. To make a button to show on the
     * toolbar, use {@link #TOOLBAR_ACTION_BUTTON_ID} as its id.
     */
    public static final String KEY_ID = "android.support.customtabs.customaction.ID";

    /**
     * The id allocated to the custom action button that is shown on the toolbar.
     */
    public static final int TOOLBAR_ACTION_BUTTON_ID = 0;

    /**
     * Extra that changes the background color for the secondary toolbar. The value should be an
     * int that specifies a {@link Color}, not a resource id.
     */
    public static final String EXTRA_SECONDARY_TOOLBAR_COLOR =
            "android.support.customtabs.extra.SECONDARY_TOOLBAR_COLOR";

    /**
     * Extra that specifies the {@link RemoteViews} showing on the secondary toolbar. If this extra
     * is set, the other secondary toolbar configurations will be overriden. The height of the
     * {@link RemoteViews} should not exceed 56dp.
     */
    public static final String EXTRA_REMOTEVIEWS =
            "android.support.customtabs.extra.EXTRA_REMOTEVIEWS";

    /**
     * Extra that specifies an array of {@link View} ids. When these {@link View}s are clicked, a
     * {@link PendingIntent} will be sent, carrying the current url of the custom tab.
     * <p>
     * Note Custom Tabs will override the default onClick behavior of the listed {@link View}s. If
     * you do not care about the current url, you can safely ignore this extra and use
     * {@link RemoteViews#setOnClickPendingIntent(int, PendingIntent)} instead.
     */
    public static final String EXTRA_REMOTEVIEWS_VIEW_IDS =
            "android.support.customtabs.extra.EXTRA_REMOTEVIEWS_VIEW_IDS";

    /**
     * Extra that specifies the {@link PendingIntent} to be sent when the user clicks on the
     * {@link View}s that is listed by {@link #EXTRA_REMOTEVIEWS_CLICKED_ID}.
     * <p>
     * Note when this {@link PendingIntent} is triggered, it will have the current url as data
     * field, also the id of the clicked {@link View}, specified by
     * {@link #EXTRA_REMOTEVIEWS_CLICKED_ID}.
     */
    public static final String EXTRA_REMOTEVIEWS_PENDINGINTENT =
            "android.support.customtabs.extra.EXTRA_REMOTEVIEWS_PENDINGINTENT";

    /**
     * Extra that specifies which {@link View} has been clicked. This extra will be put to the
     * {@link PendingIntent} sent from Custom Tabs when a view in the {@link RemoteViews} is clicked
     */
    public static final String EXTRA_REMOTEVIEWS_CLICKED_ID =
            "android.support.customtabs.extra.EXTRA_REMOTEVIEWS_CLICKED_ID";

    /**
     * Convenience method to create a VIEW intent without a session for the given package.
     * @param packageName The package name to set in the intent.
     * @param data        The data {@link Uri} to be used in the intent.
     * @return            The intent with the given package, data and the right session extra.
     */
    public static Intent getViewIntentWithNoSession(String packageName, Uri data) {
        Intent intent = new Intent(Intent.ACTION_VIEW, data);
        intent.setPackage(packageName);
        Bundle extras = new Bundle();
        if (!safePutBinder(extras, EXTRA_SESSION, null)) return null;
        intent.putExtras(extras);
        return intent;
    }

    /**
     * A convenience method to handle putting an {@link IBinder} inside a {@link Bundle} for all
     * Android version.
     * @param bundle The bundle to insert the {@link IBinder}.
     * @param key    The key to use while putting the {@link IBinder}.
     * @param binder The {@link IBinder} to put.
     * @return       Whether the operation was successful.
     */
    static boolean safePutBinder(Bundle bundle, String key, IBinder binder) {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
                bundle.putBinder(key, binder);
            } else {
                Method putBinderMethod =
                        Bundle.class.getMethod("putIBinder", String.class, IBinder.class);
                putBinderMethod.invoke(bundle, key, binder);
            }
        } catch (InvocationTargetException | IllegalAccessException | IllegalArgumentException
                | NoSuchMethodException e) {
            return false;
        }
        return true;
    }
}
