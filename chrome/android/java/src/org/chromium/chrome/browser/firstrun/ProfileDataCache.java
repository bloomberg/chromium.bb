// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;

import org.chromium.base.ObserverList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileDownloader;

import java.util.HashMap;
import java.util.List;

/**
 * Fetches and caches Google Account profile images and full names for the accounts on the device.
 * ProfileDataCache doesn't observe account list changes by itself, so account list
 * should be provided by calling {@link #update(List)}
 */
public class ProfileDataCache implements ProfileDownloader.Observer {
    private static class CacheEntry {
        public CacheEntry(Bitmap picture, String fullName, String givenName) {
            this.picture = picture;
            this.fullName = fullName;
            this.givenName = givenName;
        }

        public Bitmap picture;
        public String fullName;
        public String givenName;
    }

    private final HashMap<String, CacheEntry> mCacheEntries = new HashMap<>();

    private final Bitmap mPlaceholderImage;
    private final ObserverList<ProfileDownloader.Observer> mObservers = new ObserverList<>();

    private final Context mContext;
    private Profile mProfile;

    public ProfileDataCache(Context context, Profile profile) {
        mContext = context;
        mProfile = profile;

        Bitmap placeHolder = BitmapFactory.decodeResource(mContext.getResources(),
                R.drawable.fre_placeholder);
        mPlaceholderImage = getCroppedBitmap(placeHolder);

        ProfileDownloader.addObserver(this);
    }

    /**
     * Initiate fetching the user accounts data (images and the full name).
     * Fetched data will be sent to observers of ProfileDownloader.
     */
    public void update(List<String> accounts) {
        int imageSizePx =
                mContext.getResources().getDimensionPixelSize(R.dimen.signin_account_image_size);
        for (int i = 0; i < accounts.size(); i++) {
            if (mCacheEntries.get(accounts.get(i)) == null) {
                ProfileDownloader.startFetchingAccountInfoFor(
                        mContext, mProfile, accounts.get(i), imageSizePx, true);
            }
        }
    }

    /**
     * @param accountId Google account ID for the image that is requested.
     * @return Returns the profile image for a given Google account ID if it's in
     *         the cache, otherwise returns a placeholder image.
     */
    public Bitmap getImage(String accountId) {
        CacheEntry cacheEntry = mCacheEntries.get(accountId);
        if (cacheEntry == null) return mPlaceholderImage;
        return cacheEntry.picture;
    }

    /**
     * @param accountId Google account ID for the full name that is requested.
     * @return Returns the full name for a given Google account ID if it is
     *         the cache, otherwise returns null.
     */
    public String getFullName(String accountId) {
        CacheEntry cacheEntry = mCacheEntries.get(accountId);
        if (cacheEntry == null) return null;
        return cacheEntry.fullName;
    }

    /**
     * @param accountId Google account ID for the full name that is requested.
     * @return Returns the given name for a given Google account ID if it is in the cache, otherwise
     * returns null.
     */
    public String getGivenName(String accountId) {
        CacheEntry cacheEntry = mCacheEntries.get(accountId);
        if (cacheEntry == null) return null;
        return cacheEntry.givenName;
    }

    public void destroy() {
        ProfileDownloader.removeObserver(this);
        mObservers.clear();
    }

    /**
     * @param observer Observer that should be notified when new profile images are available.
     */
    public void addObserver(ProfileDownloader.Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * @param observer Observer that was added by {@link #addObserver} and should be removed.
     */
    public void removeObserver(ProfileDownloader.Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void onProfileDownloaded(String accountId, String fullName, String givenName,
            Bitmap bitmap) {
        bitmap = getCroppedBitmap(bitmap);
        mCacheEntries.put(accountId, new CacheEntry(bitmap, fullName, givenName));
        for (ProfileDownloader.Observer observer : mObservers) {
            observer.onProfileDownloaded(accountId, fullName, givenName, bitmap);
        }
    }

    private Bitmap getCroppedBitmap(Bitmap bitmap) {
        Bitmap output = Bitmap.createBitmap(
                bitmap.getWidth(), bitmap.getHeight(), Config.ARGB_8888);
        Canvas canvas = new Canvas(output);

        final Paint paint = new Paint();
        final Rect rect = new Rect(0, 0, bitmap.getWidth(), bitmap.getHeight());

        paint.setAntiAlias(true);
        canvas.drawARGB(0, 0, 0, 0);
        paint.setColor(Color.WHITE);

        canvas.drawCircle(
                bitmap.getWidth() / 2f, bitmap.getHeight() / 2f, bitmap.getWidth() / 2f, paint);
        paint.setXfermode(new PorterDuffXfermode(Mode.SRC_IN));
        canvas.drawBitmap(bitmap, rect, rect, paint);

        return output;
    }
}
