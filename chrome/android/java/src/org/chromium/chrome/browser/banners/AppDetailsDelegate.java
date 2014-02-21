// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.content.Context;
import android.os.AsyncTask;

/**
 * Fetches data about the given app.
 */
public abstract class AppDetailsDelegate {
    /**
     * Class to inform when the app's details have been retrieved.
     */
    interface Observer {
        /**
         * Called when the task has finished.
         * @param data Data about the requested package.  Will be null if retrieval failed.
         */
        public void onAppDetailsRetrieved(AppData data);
    }

    /**
     * Runs a background task to retrieve details about an app.
     */
    class DetailsTask extends AsyncTask<Void, Void, Boolean> {
        final Observer mObserver;
        final AppData mData;
        final int mIconSize;

        DetailsTask(Observer observer, Context context, String url, String packageName) {
            mObserver = observer;
            mData = new AppData(url, packageName);
            mIconSize = AppBannerView.getIconSize(context);
        }

        @Override
        protected Boolean doInBackground(Void... args) {
            return getAppDetails(mData, mIconSize);
        }

        @Override
        protected void onPostExecute(Boolean result) {
            mObserver.onAppDetailsRetrieved(result ? mData : null);
        }
    }

    /**
     * Creates a task to retrieve data about the given app.
     * @param observer    Observer to inform about the task's completion.
     * @param context     Context to grab resources from.
     * @param url         URL of the page that is requesting a banner.
     * @param packageName Package name for the app being displayed.
     * @return
     */
    DetailsTask createTask(Observer observer, Context context, String url, String packageName) {
        return new DetailsTask(observer, context, url, packageName);
    }

    /**
     * Retrieves information about the given package.
     * @param data        Data about the app will be placed in this class.  Must already contain
     *                    the name of the package to retrieve data for.
     * @param iconSize    Size of the icon to retrieve.
     * @return            True if the data was retrieved successfully.
     */
    protected abstract boolean getAppDetails(AppData data, int iconSize);

    /**
     * Destroy the delegate, cleaning up any open hooks.
     */
    public abstract void destroy();
}
