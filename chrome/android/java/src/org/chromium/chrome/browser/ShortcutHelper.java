// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.util.Base64;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.webapps.WebappAuthenticator;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.content_public.common.ScreenOrientationConstants;
import org.chromium.ui.widget.Toast;

import java.io.ByteArrayOutputStream;
import java.util.List;

/**
 * This class contains functions related to adding shortcuts to the Android Home
 * screen.  These shortcuts are used to either open a page in the main browser
 * or open a web app.
 */
public class ShortcutHelper {
    public static final String EXTRA_ICON = "org.chromium.chrome.browser.webapp_icon";
    public static final String EXTRA_ID = "org.chromium.chrome.browser.webapp_id";
    public static final String EXTRA_MAC = "org.chromium.chrome.browser.webapp_mac";
    // EXTRA_TITLE is present for backward compatibility reasons
    public static final String EXTRA_TITLE = "org.chromium.chrome.browser.webapp_title";
    public static final String EXTRA_NAME = "org.chromium.chrome.browser.webapp_name";
    public static final String EXTRA_SHORT_NAME = "org.chromium.chrome.browser.webapp_short_name";
    public static final String EXTRA_URL = "org.chromium.chrome.browser.webapp_url";
    public static final String EXTRA_SCOPE = "org.chromium.chrome.browser.webapp_scope";
    public static final String EXTRA_ORIENTATION = ScreenOrientationConstants.EXTRA_ORIENTATION;
    public static final String EXTRA_SOURCE = "org.chromium.chrome.browser.webapp_source";
    public static final String EXTRA_THEME_COLOR = "org.chromium.chrome.browser.theme_color";
    public static final String EXTRA_BACKGROUND_COLOR =
            "org.chromium.chrome.browser.background_color";
    public static final String EXTRA_IS_ICON_GENERATED =
            "org.chromium.chrome.browser.is_icon_generated";
    public static final String EXTRA_VERSION =
            "org.chromium.chrome.browser.webapp_shortcut_version";
    public static final String REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB =
            "REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB";

    // When a new field is added to the intent, this version should be incremented so that it will
    // be correctly populated into the WebappRegistry/WebappDataStorage.
    public static final int WEBAPP_SHORTCUT_VERSION = 1;

    // This value is equal to kInvalidOrMissingColor in the C++ content::Manifest struct.
    public static final long MANIFEST_COLOR_INVALID_OR_MISSING = ((long) Integer.MAX_VALUE) + 1;

    private static final String TAG = "ShortcutHelper";

    // There is no public string defining this intent so if Home changes the value, we
    // have to update this string.
    private static final String INSTALL_SHORTCUT = "com.android.launcher.action.INSTALL_SHORTCUT";

    // These sizes are from the Material spec for icons:
    // https://www.google.com/design/spec/style/icons.html#icons-product-icons
    private static final float MAX_INNER_SIZE_RATIO = 1.25f;
    private static final float ICON_PADDING_RATIO = 2.0f / 44.0f;
    private static final float ICON_CORNER_RADIUS_RATIO = 1.0f / 16.0f;
    private static final float GENERATED_ICON_PADDING_RATIO = 1.0f / 12.0f;
    private static final float GENERATED_ICON_FONT_SIZE_RATIO = 1.0f / 3.0f;

    /** Broadcasts Intents out Android for adding the shortcut. */
    public static class Delegate {
        /**
         * Broadcasts an intent to all interested BroadcastReceivers.
         * @param context The Context to use.
         * @param intent The intent to broadcast.
         */
        public void sendBroadcast(Context context, Intent intent) {
            context.sendBroadcast(intent);
        }

        /**
         * Returns the name of the fullscreen Activity to use when launching shortcuts.
         */
        public String getFullscreenAction() {
            return WebappLauncherActivity.ACTION_START_WEBAPP;
        }
    }

    private static Delegate sDelegate = new Delegate();

    /**
     * Sets the delegate to use.
     */
    @VisibleForTesting
    public static void setDelegateForTests(Delegate delegate) {
        sDelegate = delegate;
    }

    /**
     * Called when we have to fire an Intent to add a shortcut to the home screen.
     * If the webpage indicated that it was capable of functioning as a webapp, it is added as a
     * shortcut to a webapp Activity rather than as a general bookmark. User is sent to the
     * home screen as soon as the shortcut is created.
     *
     * This method must not be called on the UI thread.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static void addShortcut(Context context, String id, String url, final String userTitle,
            String name, String shortName, Bitmap icon, boolean isWebappCapable, int orientation,
            int source, long themeColor, long backgroundColor, boolean isIconGenerated,
            final long callbackPointer) {
        assert !ThreadUtils.runningOnUiThread();
        final Intent shortcutIntent;
        if (isWebappCapable) {
            shortcutIntent = createWebappShortcutIntent(id, sDelegate.getFullscreenAction(), url,
                    getScopeFromUrl(url), name, shortName, icon, WEBAPP_SHORTCUT_VERSION,
                    orientation, themeColor, backgroundColor, isIconGenerated);
            shortcutIntent.putExtra(EXTRA_MAC, getEncodedMac(context, url));
        } else {
            // Add the shortcut as a launcher icon to open in the browser Activity.
            shortcutIntent = createShortcutIntent(url);
        }

        // Always attach a source (one of add to home screen menu item, app banner, or unknown) to
        // the intent. This allows us to distinguish where a shortcut was added from in metrics.
        shortcutIntent.putExtra(EXTRA_SOURCE, source);
        shortcutIntent.setPackage(context.getPackageName());
        sDelegate.sendBroadcast(
                context, createAddToHomeIntent(url, userTitle, icon, shortcutIntent));

        if (isWebappCapable) {
            // Store the webapp data so that it is accessible without the intent. Once this process
            // is complete, call back to native code to start the splash image download.
            WebappRegistry.registerWebapp(context, id,
                    new WebappRegistry.FetchWebappDataStorageCallback() {
                        @Override
                        public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                            storage.updateFromShortcutIntent(shortcutIntent);
                            nativeOnWebappDataStored(callbackPointer);
                        }
                    }
            );
        }

        // Alert the user about adding the shortcut.
        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(new Runnable() {
            @Override
            public void run() {
                Context applicationContext = ContextUtils.getApplicationContext();
                String toastText =
                        applicationContext.getString(R.string.added_to_homescreen, userTitle);
                Toast toast = Toast.makeText(applicationContext, toastText, Toast.LENGTH_SHORT);
                toast.show();
            }
        });
    }

    /**
     * Creates a storage location and stores the data for a web app using {@link WebappDataStorage}.
     * @param context     Context to open the WebappDataStorage with.
     * @param id          ID of the webapp which is storing data.
     * @param splashImage Image which should be displayed on the splash screen of
     *                    the webapp. This can be null of there is no image to show.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static void storeWebappSplashImage(final Context context, final String id,
            final Bitmap splashImage) {
        WebappRegistry.getWebappDataStorage(context, id,
                new WebappRegistry.FetchWebappDataStorageCallback() {
                    @Override
                    public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                        if (storage == null) return;

                        storage.updateSplashScreenImage(splashImage);
                    }

                }
        );
    }

    /**
     * Creates an intent that will add a shortcut to the home screen.
     * @param shortcutIntent Intent to fire when the shortcut is activated.
     * @param url URL of the shortcut.
     * @param title Title of the shortcut.
     * @param icon Image that represents the shortcut.
     * @return Intent for the shortcut.
     */
    public static Intent createAddToHomeIntent(String url, String title,
            Bitmap icon, Intent shortcutIntent) {
        Intent i = new Intent(INSTALL_SHORTCUT);
        i.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
        i.putExtra(Intent.EXTRA_SHORTCUT_NAME, title);
        i.putExtra(Intent.EXTRA_SHORTCUT_ICON, icon);
        return i;
    }

    /**
     * Creates an intent that will add a shortcut to the home screen.
     * @param url Url of the shortcut.
     * @param title Title of the shortcut.
     * @param icon Image that represents the shortcut.
     * @return Intent for the shortcut.
     */
    public static Intent createAddToHomeIntent(String url, String title, Bitmap icon) {
        Intent shortcutIntent = createShortcutIntent(url);
        return createAddToHomeIntent(url, title, icon, shortcutIntent);
    }

    /**
     * Creates a shortcut to launch a web app on the home screen.
     * @param id              Id of the web app.
     * @param action          Intent action to open a full screen activity.
     * @param url             Url of the web app.
     * @param scope           Url scope of the web app.
     * @param name            Name of the web app.
     * @param shortName       Short name of the web app.
     * @param icon            Icon of the web app.
     * @param version         Version number of the shortcut.
     * @param orientation     Orientation of the web app.
     * @param themeColor      Theme color of the web app.
     * @param backgroundColor Background color of the web app.
     * @param isIconGenerated True if the icon is generated by Chromium.
     * @return Intent for onclick action of the shortcut.
     */
    public static Intent createWebappShortcutIntent(String id, String action, String url,
            String scope, String name, String shortName, Bitmap icon, int version, int orientation,
            long themeColor, long backgroundColor, boolean isIconGenerated) {
        // Encode the icon as a base64 string (Launcher drops Bitmaps in the Intent).
        String encodedIcon = encodeBitmapAsString(icon);

        // Create an intent as a launcher icon for a full-screen Activity.
        Intent shortcutIntent = new Intent();
        shortcutIntent.setAction(action)
                .putExtra(EXTRA_ID, id)
                .putExtra(EXTRA_URL, url)
                .putExtra(EXTRA_SCOPE, scope)
                .putExtra(EXTRA_NAME, name)
                .putExtra(EXTRA_SHORT_NAME, shortName)
                .putExtra(EXTRA_ICON, encodedIcon)
                .putExtra(EXTRA_VERSION, version)
                .putExtra(EXTRA_ORIENTATION, orientation)
                .putExtra(EXTRA_THEME_COLOR, themeColor)
                .putExtra(EXTRA_BACKGROUND_COLOR, backgroundColor)
                .putExtra(EXTRA_IS_ICON_GENERATED, isIconGenerated);
        return shortcutIntent;
    }

    /**
     * Shortcut intent for icon on home screen.
     * @param url Url of the shortcut.
     * @return Intent for onclick action of the shortcut.
     */
    public static Intent createShortcutIntent(String url) {
        Intent shortcutIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        shortcutIntent.putExtra(REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        return shortcutIntent;
    }

    /**
     * Utility method to check if a shortcut can be added to the home screen.
     * @param context Context used to get the package manager.
     * @return if a shortcut can be added to the home screen under the current profile.
     */
    public static boolean isAddToHomeIntentSupported(Context context) {
        PackageManager pm = context.getPackageManager();
        Intent i = new Intent(INSTALL_SHORTCUT);
        List<ResolveInfo> receivers = pm.queryBroadcastReceivers(
                i, PackageManager.GET_INTENT_FILTERS);
        return !receivers.isEmpty();
    }

    /**
     * Returns whether the given icon matches the size requirements to be used on the home screen.
     * @param context Context used to create the intent.
     * @param width Icon width, in pixels.
     * @param height Icon height, in pixels.
     * @return whether the given icon matches the size requirements to be used on the home screen.
     */
    @CalledByNative
    public static boolean isIconLargeEnoughForLauncher(Context context, int width, int height) {
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        final int minimalSize = am.getLauncherLargeIconSize() / 2;
        return width >= minimalSize && height >= minimalSize;
    }

    /**
     * Adapts a website's icon (e.g. favicon or touch icon) to the Material design style guidelines
     * for home screen icons. This involves adding some padding and rounding the corners.
     *
     * @param context Context used to create the intent.
     * @param webIcon The website's favicon or touch icon.
     * @return Bitmap Either the touch-icon or the newly created favicon.
     */
    @CalledByNative
    public static Bitmap createHomeScreenIconFromWebIcon(Context context, Bitmap webIcon) {
        // getLauncherLargeIconSize() is just a guess at the launcher icon size, and is often
        // wrong -- the launcher can show icons at any size it pleases. Instead of resizing the
        // icon to the supposed launcher size and then having the launcher resize the icon again,
        // just leave the icon at its original size and let the launcher do a single rescaling.
        // Unless the icon is much too big; then scale it down here too.
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        int maxInnerSize = Math.round(am.getLauncherLargeIconSize() * MAX_INNER_SIZE_RATIO);
        int innerSize = Math.min(maxInnerSize, Math.max(webIcon.getWidth(), webIcon.getHeight()));
        int padding = Math.round(ICON_PADDING_RATIO * innerSize);
        int outerSize = innerSize + 2 * padding;

        Bitmap bitmap = null;
        try {
            bitmap = Bitmap.createBitmap(outerSize, outerSize, Bitmap.Config.ARGB_8888);
        } catch (OutOfMemoryError e) {
            Log.w(TAG, "OutOfMemoryError while creating bitmap for home screen icon.");
            return webIcon;
        }

        // Draw the web icon with padding around it.
        Canvas canvas = new Canvas(bitmap);
        Rect innerBounds = new Rect(padding, padding, outerSize - padding, outerSize - padding);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setFilterBitmap(true);
        canvas.drawBitmap(webIcon, null, innerBounds, paint);

        // Round the corners.
        int cornerRadius = Math.round(ICON_CORNER_RADIUS_RATIO * outerSize);
        Path path = new Path();
        path.setFillType(Path.FillType.INVERSE_WINDING);
        RectF innerBoundsF = new RectF(innerBounds);
        path.addRoundRect(innerBoundsF, cornerRadius, cornerRadius, Path.Direction.CW);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
        canvas.drawPath(path, paint);

        return bitmap;
    }

    /**
     * Generates a generic icon to be used in the launcher. This is just a rounded rectangle with
     * a letter in the middle taken from the website's domain name.
     *
     * @param context Context used to create the intent.
     * @param url URL of the shortcut.
     * @param red Red component of the dominant icon color.
     * @param green Green component of the dominant icon color.
     * @param blue Blue component of the dominant icon color.
     * @return Bitmap Either the touch-icon or the newly created favicon.
     */
    @CalledByNative
    public static Bitmap generateHomeScreenIcon(Context context, String url, int red, int green,
            int blue) {
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        final int outerSize = am.getLauncherLargeIconSize();
        final int iconDensity = am.getLauncherLargeIconDensity();

        Bitmap bitmap = null;
        try {
            bitmap = Bitmap.createBitmap(outerSize, outerSize, Bitmap.Config.ARGB_8888);
        } catch (OutOfMemoryError e) {
            Log.w(TAG, "OutOfMemoryError while trying to draw bitmap on canvas.");
            return null;
        }

        Canvas canvas = new Canvas(bitmap);

        // Draw the drop shadow.
        int padding = (int) (GENERATED_ICON_PADDING_RATIO * outerSize);
        Rect outerBounds = new Rect(0, 0, outerSize, outerSize);
        Bitmap iconShadow =
                getBitmapFromResourceId(context, R.mipmap.shortcut_icon_shadow, iconDensity);
        Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG);
        canvas.drawBitmap(iconShadow, null, outerBounds, paint);

        // Draw the rounded rectangle and letter.
        int innerSize = outerSize - 2 * padding;
        int cornerRadius = Math.round(ICON_CORNER_RADIUS_RATIO * outerSize);
        int fontSize = Math.round(GENERATED_ICON_FONT_SIZE_RATIO * outerSize);
        int color = Color.rgb(red, green, blue);
        RoundedIconGenerator generator = new RoundedIconGenerator(
                innerSize, innerSize, cornerRadius, color, fontSize);
        Bitmap icon = generator.generateIconForUrl(url);
        if (icon == null) return null; // Bookmark URL does not have a domain.
        canvas.drawBitmap(icon, padding, padding, null);

        return bitmap;
    }

    /**
     * Compresses a bitmap into a PNG and converts into a Base64 encoded string.
     * The encoded string can be decoded using {@link decodeBitmapFromString(String)}.
     * @param bitmap The Bitmap to compress and encode.
     * @return the String encoding the Bitmap.
     */
    public static String encodeBitmapAsString(Bitmap bitmap) {
        if (bitmap == null) return "";
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, output);
        return Base64.encodeToString(output.toByteArray(), Base64.DEFAULT);
    }

    /**
     * Decodes a Base64 string into a Bitmap. Used to decode Bitmaps encoded by
     * {@link encodeBitmapAsString(Bitmap)}.
     * @param encodedString the Base64 String to decode.
     * @return the Bitmap which was encoded by the String.
     */
    public static Bitmap decodeBitmapFromString(String encodedString) {
        if (TextUtils.isEmpty(encodedString)) return null;
        byte[] decoded = Base64.decode(encodedString, Base64.DEFAULT);
        return BitmapFactory.decodeByteArray(decoded, 0, decoded.length);
    }

    /**
     * Returns the ideal size for an icon representing a web app.  This size is used on app banners,
     * the Android Home screen, and in Android's recent tasks list, among other places.
     * @param resources Resources to retrieve the dimension from.
     * @return the dimensions in dp which the icon should have.
     */
    public static int getIdealHomescreenIconSizeInDp(Context context) {
        return getIdealSizeFromResourceInDp(context, R.dimen.webapp_home_screen_icon_size);
    }

    /**
     * Returns the minimum size for an icon representing a web app.  This size is used on app
     * banners, the Android Home screen, and in Android's recent tasks list, among other places.
     * @param resources Resources to retrieve the dimension from.
     * @return the lower bound of the size which the icon should have in dp.
     */
    public static int getMinimumHomescreenIconSizeInDp(Context context) {
        float sizeInPx = context.getResources().getDimension(R.dimen.webapp_home_screen_icon_size);
        float density = context.getResources().getDisplayMetrics().density;
        float idealIconSizeInDp = sizeInPx / density;

        float minimumIconSizeInPx = idealIconSizeInDp * (density - 1);
        return Math.round(minimumIconSizeInPx / density);
    }

    /**
     * Returns the ideal size for an image displayed on a web app's splash screen.
     * @param resources Resources to retrieve the dimension from.
     * @return the dimensions in dp which the image should have.
     */
    public static int getIdealSplashImageSizeInDp(Context context) {
        return getIdealSizeFromResourceInDp(context, R.dimen.webapp_splash_image_size_ideal);
    }

    /**
     * Returns the minimum size for an image displayed on a web app's splash screen.
     * @param resources Resources to retrieve the dimension from.
     * @return the lower bound of the size which the image should have in dp.
     */
    public static int getMinimumSplashImageSizeInDp(Context context) {
        return getIdealSizeFromResourceInDp(context, R.dimen.webapp_splash_image_size_minimum);
    }

    /**
     * @return String that can be used to verify that a WebappActivity is being started by Chrome.
     */
    public static String getEncodedMac(Context context, String url) {
        // The only reason we convert to a String here is because Android inexplicably eats a
        // byte[] when adding the shortcut -- the Bundle received by the launched Activity even
        // lacks the key for the extra.
        byte[] mac = WebappAuthenticator.getMacForUrl(context, url);
        return Base64.encodeToString(mac, Base64.DEFAULT);
    }

    /**
     * Returns the URL with all but the last component of its path removed. This serves as a proxy
     * for scope until the scope manifest member is available. This method assumes that the URL
     * passed in is a valid URL with a path that contains at least one "/".
     * @param url The url to convert to a scope.
     * @return The scope.
     */
    public static String getScopeFromUrl(String url) {
        Uri uri = Uri.parse(url);
        List<String> path = uri.getPathSegments();
        int endIndex = path.size();

        // If there is at least one path element, remove the last one.
        if (endIndex > 0) {
            endIndex -= 1;
        }

        // Make sure the path starts and ends with a slash (or is only a slash if there is no path).
        Uri.Builder builder = uri.buildUpon();
        String scope_path = "/" + TextUtils.join("/", path.subList(0, endIndex));
        if (scope_path.length() > 1) {
            scope_path += "/";
        }
        builder.path(scope_path);

        // Clear out the query and fragment.
        builder.fragment("");
        builder.query("");
        return builder.build().toString();
    }

    /**
     * Returns an array of sizes which describe the ideal size and minimum size of the Home screen
     * icon and the ideal and minimum sizes of the splash screen image in that order.
     */
    @CalledByNative
    private static int[] getHomeScreenIconAndSplashImageSizes(Context context) {
        // This ordering must be kept up to date with the C++ ShortcutHelper.
        return new int[] {
            getIdealHomescreenIconSizeInDp(context),
            getMinimumHomescreenIconSizeInDp(context),
            getIdealSplashImageSizeInDp(context),
            getMinimumSplashImageSizeInDp(context)
        };
    }

    private static int getIdealSizeFromResourceInDp(Context context, int resource) {
        float sizeInPx = context.getResources().getDimension(resource);
        float density = context.getResources().getDisplayMetrics().density;
        return Math.round(sizeInPx / density);
    }

    private static Bitmap getBitmapFromResourceId(Context context, int id, int density) {
        Drawable drawable = ApiCompatibilityUtils.getDrawableForDensity(
                context.getResources(), id, density);

        if (drawable instanceof BitmapDrawable) {
            BitmapDrawable bd = (BitmapDrawable) drawable;
            return bd.getBitmap();
        }
        assert false : "The drawable was not a bitmap drawable as expected";
        return null;
    }

    private static native void nativeOnWebappDataStored(long callbackPointer);
}
