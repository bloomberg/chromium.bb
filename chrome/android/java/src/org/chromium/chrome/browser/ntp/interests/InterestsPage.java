// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.interests;

import android.content.Context;
import android.view.View;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.NativePage;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.interests.InterestsService.GetInterestsCallback;
import org.chromium.chrome.browser.ntp.interests.InterestsService.Interest;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.widget.Toast;

import java.util.Arrays;
import java.util.List;

/**
 * Displays a list of topics that we think the user is interested in. When an interest is clicked
 * the user is redirected to a search for that subject.
 */
public class InterestsPage implements NativePage {

    private static final String TAG = "interests_page";

    private final InterestsView mPageView;
    private final String mTitle;
    private final int mBackgroundColor;
    private final int mThemeColor;

    /**
     * Creates an InterestsPage.
     *
     * @param context The view context for showing the page.
     * @param tab The tab from which interests page is loaded.
     * @param profile The profile from which to load interests.
     */
    public InterestsPage(final Context context, Tab tab, Profile profile) {
        mTitle = context.getResources().getString(R.string.ntp_interests);
        mBackgroundColor = ApiCompatibilityUtils.getColor(context.getResources(), R.color.ntp_bg);
        mThemeColor = ApiCompatibilityUtils.getColor(
                context.getResources(), R.color.default_primary_color);
        mPageView = (InterestsView) View.inflate(context, R.layout.interests_page, null);

        new InterestsService(profile).getInterests(
                new GetInterestsCallback() {
                    @Override
                    public void onInterestsAvailable(Interest[] interests) {
                        if (interests == null) {
                            Toast toast = Toast.makeText(context,
                                    R.string.ntp_no_interests_toast, Toast.LENGTH_SHORT);
                            toast.show();
                            return;
                        }
                        List<Interest> interestList = Arrays.asList(interests);

                        mPageView.setInterests(interestList);
                    }
                });
    }

    public void setListener(InterestsClickListener listener) {
        mPageView.setListener(listener);
    }

    /**
     * Interface to be notified when the user clicks on an interest.
     **/
    public interface InterestsClickListener {
        /**
         * Called when an interest is selected.
         *
         * @param name The name of the selected interest.
         */
        void onInterestClicked(String name);
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public int getBackgroundColor() {
        return mBackgroundColor;
    }

    @Override
    public int getThemeColor() {
        return mThemeColor;
    }
    @Override
    public View getView() {
        return mPageView;
    }


    @Override
    public void destroy() {
    }

    @Override
    public String getHost() {
        return UrlConstants.INTERESTS_HOST;
    }

    @Override
    public String getUrl() {
        return UrlConstants.INTERESTS_URL;
    }

    @Override
    public void updateForUrl(String url) {
    }
}
