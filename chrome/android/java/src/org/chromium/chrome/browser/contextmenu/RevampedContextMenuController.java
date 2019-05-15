// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Shader;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.support.annotation.ColorInt;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.v7.app.AlertDialog;
import android.text.SpannableString;
import android.text.TextUtils;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.favicon.IconType;
import org.chromium.chrome.browser.night_mode.GlobalNightModeStateProviderHolder;
import org.chromium.chrome.browser.omnibox.OmniboxUrlEmphasizer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.ContextMenuDialog;
import org.chromium.components.security_state.ConnectionSecurityLevel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * A customized context menu that displays the options in a list with the first item being
 * the header and represents the the options in groups.
 */
public class RevampedContextMenuController
        implements ContextMenuUi, AdapterView.OnItemClickListener {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ListItemType.DIVIDER, ListItemType.HEADER, ListItemType.CONTEXT_MENU_ITEM})
    public @interface ListItemType {
        int DIVIDER = 0;
        int HEADER = 1;
        int CONTEXT_MENU_ITEM = 2;

        // Keep this up-to-date
        int NUM_ENTRIES = 3;
    }

    private ContextMenuDialog mContextMenuDialog;
    private RevampedContextMenuListAdapter mListAdapter;
    private Callback<Integer> mItemClickCallback;
    private float mTopContentOffsetPx;
    private String mPlainUrl;

    /**
     * Constructor that also sets the content offset.
     *
     * @param topContentOffsetPx content offset from the top.
     */
    RevampedContextMenuController(float topContentOffsetPx) {
        mTopContentOffsetPx = topContentOffsetPx;
    }

    @Override
    public void displayMenu(final Activity activity, ContextMenuParams params,
            List<Pair<Integer, List<ContextMenuItem>>> items, Callback<Integer> onItemClicked,
            final Runnable onMenuShown, final Runnable onMenuClosed) {
        mItemClickCallback = onItemClicked;
        final Resources resources = activity.getResources();
        final float density = resources.getDisplayMetrics().density;
        final float touchPointXPx = params.getTriggeringTouchXDp() * density;
        final float touchPointYPx = params.getTriggeringTouchYDp() * density;

        final View view =
                LayoutInflater.from(activity).inflate(R.layout.revamped_context_menu, null);
        mContextMenuDialog = createContextMenuDialog(activity, view, touchPointXPx, touchPointYPx);
        mContextMenuDialog.setOnShowListener(dialogInterface -> onMenuShown.run());
        mContextMenuDialog.setOnDismissListener(dialogInterface -> onMenuClosed.run());

        // Integer here indicates if the item is the header, a divider, or a row (selectable option)
        List<Pair<Integer, ContextMenuItem>> itemList = new ArrayList<>();
        itemList.add(new Pair<>(ListItemType.HEADER, null));

        for (Pair<Integer, List<ContextMenuItem>> group : items) {
            // Add a divider
            itemList.add(new Pair<>(ListItemType.DIVIDER, null));

            for (ContextMenuItem item : group.second) {
                itemList.add(new Pair<>(ListItemType.CONTEXT_MENU_ITEM, item));
            }
        }

        String headerTitle = params.getTitleText();
        if (TextUtils.isEmpty(headerTitle)) {
            headerTitle = params.getLinkText();
        }

        mPlainUrl = params.getUrl();
        CharSequence url = params.getUrl();
        if (!TextUtils.isEmpty(url)) {
            boolean useDarkColors =
                    !GlobalNightModeStateProviderHolder.getInstance().isInNightMode();
            if (activity instanceof ChromeBaseAppCompatActivity) {
                useDarkColors = !((ChromeBaseAppCompatActivity) activity)
                                         .getNightModeStateProvider()
                                         .isInNightMode();
            }

            SpannableString spannableUrl =
                    new SpannableString(ChromeContextMenuPopulator.createUrlText(params));
            OmniboxUrlEmphasizer.emphasizeUrl(spannableUrl, resources, Profile.getLastUsedProfile(),
                    ConnectionSecurityLevel.NONE, false, useDarkColors, false);
            url = spannableUrl;
        }

        mListAdapter = new RevampedContextMenuListAdapter(activity, itemList);
        mListAdapter.setHeaderTitle(headerTitle);
        mListAdapter.setHeaderUrl(url);
        RevampedContextMenuListView listView = view.findViewById(R.id.context_menu_list_view);
        listView.setAdapter(mListAdapter);
        listView.setOnItemClickListener(this);

        mContextMenuDialog.show();
    }

    /**
     * Returns the fully complete dialog based off the params and the itemGroups.
     *
     * @param activity Used to inflate the dialog.
     * @param view The inflated view that contains the list view.
     * @param touchPointYPx The x-coordinate of the touch that triggered the context menu.
     * @param touchPointXPx The y-coordinate of the touch that triggered the context menu.
     * @return Returns a final dialog that does not have a background can be displayed using
     *         {@link AlertDialog#show()}.
     */
    private ContextMenuDialog createContextMenuDialog(
            Activity activity, View view, float touchPointXPx, float touchPointYPx) {
        View listView = view.findViewById(R.id.context_menu_list_view);
        final ContextMenuDialog dialog =
                new ContextMenuDialog(activity, R.style.Theme_Chromium_DialogWhenLarge,
                        touchPointXPx, touchPointYPx, mTopContentOffsetPx, listView);
        dialog.setContentView(view);

        return dialog;
    }

    /**
     * This adds a checkerboard style background to the image.
     * It is useful for the transparent PNGs.
     * @return The given image with the checkerboard pattern in the background.
     */
    static Bitmap getImageWithCheckerBackground(Resources res, Bitmap image) {
        // 1. Create a bitmap for the checkerboard pattern.
        Drawable drawable = ApiCompatibilityUtils.getDrawable(
                res, org.chromium.chrome.R.drawable.checkerboard_background);
        Bitmap tileBitmap = Bitmap.createBitmap(drawable.getIntrinsicWidth(),
                drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
        Canvas tileCanvas = new Canvas(tileBitmap);
        drawable.setBounds(0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());
        drawable.draw(tileCanvas);

        // 2. Create a BitmapDrawable using the checkerboard pattern bitmap.
        BitmapDrawable bitmapDrawable = new BitmapDrawable(res, tileBitmap);
        bitmapDrawable.setTileModeXY(Shader.TileMode.REPEAT, Shader.TileMode.REPEAT);
        bitmapDrawable.setBounds(0, 0, image.getWidth(), image.getHeight());

        // 3. Create a bitmap-backed canvas for the final image.
        Bitmap bitmap =
                Bitmap.createBitmap(image.getWidth(), image.getHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);

        // 4. Paint the checkerboard background into the final canvas
        bitmapDrawable.draw(canvas);

        // 5. Draw the image on top.
        Paint paint = new Paint();
        paint.setAntiAlias(true);
        canvas.drawBitmap(image, new Matrix(), paint);

        return bitmap;
    }

    /**
     * This is called when the thumbnail is fetched and ready to display.
     * @param thumbnail The bitmap received that will be displayed as the header image.
     */
    void onImageThumbnailRetrieved(Bitmap thumbnail) {
        mListAdapter.onImageThumbnailRetrieved(thumbnail);
    }

    /**
     * See {@link org.chromium.chrome.browser.favicon.LargeIconBridge#getLargeIconForUrl}
     */
    void onFaviconAvailable(@Nullable Bitmap icon, @ColorInt int fallbackColor,
            boolean isColorDefault, @IconType int iconType) {
        mListAdapter.onFaviconAvailable(icon, fallbackColor, mPlainUrl);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        mContextMenuDialog.dismiss();
        mItemClickCallback.onResult((int) id);
    }
}
