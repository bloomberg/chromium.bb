// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.support.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.rappor.RapporServiceBridge;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.ui.base.PageTransition;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Records UMA stats for which actions the user takes on the NTP in the
 * "NewTabPage.ActionAndroid" histogram.
 */
public class NewTabPageUma {
    // Possible actions taken by the user on the NTP. These values are also defined in
    // histograms.xml. WARNING: these values must stay in sync with histograms.xml.

    // User performed a search using the omnibox
    private static final int ACTION_SEARCHED_USING_OMNIBOX = 0;
    // User navigated to Google search homepage using the omnibox
    private static final int ACTION_NAVIGATED_TO_GOOGLE_HOMEPAGE = 1;
    // User navigated to any other page using the omnibox
    private static final int ACTION_NAVIGATED_USING_OMNIBOX = 2;
    // User opened a most visited page
    public static final int ACTION_OPENED_MOST_VISITED_ENTRY = 3;
    // User opened a recently closed tab
    public static final int ACTION_OPENED_RECENTLY_CLOSED_ENTRY = 4;
    // User opened a bookmark
    public static final int ACTION_OPENED_BOOKMARK = 5;
    // User opened a foreign session (from recent tabs section)
    public static final int ACTION_OPENED_FOREIGN_SESSION = 6;
    // User navigated to the webpage for a snippet shown on the NTP.
    public static final int ACTION_OPENED_SNIPPET = 7;
    // User clicked on an interest item.
    public static final int ACTION_CLICKED_INTEREST = 8;
    // The number of possible actions
    private static final int NUM_ACTIONS = 9;

    // User navigated to a page using the omnibox.
    private static final int RAPPOR_ACTION_NAVIGATED_USING_OMNIBOX = 0;
    // User navigated to a page using one of the suggested tiles.
    public static final int RAPPOR_ACTION_VISITED_SUGGESTED_TILE = 1;
    // The number of possible actions pertinent to Rappor
    private static final int RAPPOR_NUM_ACTIONS = 2;

    // Regular NTP impression (usually when a new tab is opened)
    public static final int NTP_IMPRESSION_REGULAR = 0;

    // Potential NTP impressions (instead of blank page if no tab is open)
    public static final int NTP_IMPESSION_POTENTIAL_NOTAB = 1;

    // The number of possible NTP impression types
    private static final int NUM_NTP_IMPRESSION = 2;

    /** Possible interactions with the snippets.
     * Do not remove or change existing values other than NUM_SNIPPETS_ACTIONS. */
    @IntDef({SNIPPETS_ACTION_SHOWN, SNIPPETS_ACTION_SCROLLED, SNIPPETS_ACTION_CLICKED,
             SNIPPETS_ACTION_DISMISSED_OBSOLETE, SNIPPETS_ACTION_DISMISSED_VISITED,
             SNIPPETS_ACTION_DISMISSED_UNVISITED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SnippetsAction {}
    /** Snippets are enabled and are being shown to the user. */
    public static final int SNIPPETS_ACTION_SHOWN = 0;
    /** The snippet list has been scrolled. */
    public static final int SNIPPETS_ACTION_SCROLLED = 1;
    /** A snippet has been clicked. */
    public static final int SNIPPETS_ACTION_CLICKED = 2;
    /** A snippet has been dismissed, made obsolete by the next two actions. */
    public static final int SNIPPETS_ACTION_DISMISSED_OBSOLETE = 3;
    /** A snippet has been swiped away, it had been viewed by the user (on this device). */
    public static final int SNIPPETS_ACTION_DISMISSED_VISITED = 4;
    /** A snippet has been swiped away, it had not been viewed by the user (on this device). */
    public static final int SNIPPETS_ACTION_DISMISSED_UNVISITED = 5;
    /** The number of possible actions. */
    private static final int NUM_SNIPPETS_ACTIONS = 6;

    /**
     * Records an action taken by the user on the NTP.
     * @param action One of the ACTION_* values defined in this class.
     */
    public static void recordAction(int action) {
        assert action >= 0;
        assert action < NUM_ACTIONS;
        switch (action) {
            case ACTION_OPENED_MOST_VISITED_ENTRY:
                RecordUserAction.record("MobileNTPMostVisited");
                break;
            case ACTION_OPENED_RECENTLY_CLOSED_ENTRY:
                RecordUserAction.record("MobileNTPRecentlyClosed");
                break;
            case ACTION_OPENED_BOOKMARK:
                RecordUserAction.record("MobileNTPBookmark");
                break;
            case ACTION_OPENED_FOREIGN_SESSION:
                RecordUserAction.record("MobileNTPForeignSession");
                break;
            default:
                // No UMA action associated with this type.
                break;
        }
        RecordHistogram.recordEnumeratedHistogram("NewTabPage.ActionAndroid", action, NUM_ACTIONS);
    }

    /**
     * Record that the user has navigated away from the NTP using the omnibox.
     * @param destinationUrl The URL to which the user navigated.
     * @param transitionType The transition type of the navigation, from PageTransition.java.
     */
    public static void recordOmniboxNavigation(String destinationUrl, int transitionType) {
        if ((transitionType & PageTransition.CORE_MASK) == PageTransition.GENERATED) {
            recordAction(ACTION_SEARCHED_USING_OMNIBOX);
        } else {
            if (UrlUtilities.nativeIsGoogleHomePageUrl(destinationUrl)) {
                recordAction(ACTION_NAVIGATED_TO_GOOGLE_HOMEPAGE);
            } else {
                recordAction(ACTION_NAVIGATED_USING_OMNIBOX);
            }
            recordExplicitUserNavigation(destinationUrl, RAPPOR_ACTION_NAVIGATED_USING_OMNIBOX);
        }
    }

    /**
     * Record the eTLD+1 for a website explicitly visited by the user, using Rappor.
     */
    public static void recordExplicitUserNavigation(String destinationUrl, int rapporMetric) {
        switch (rapporMetric) {
            case RAPPOR_ACTION_NAVIGATED_USING_OMNIBOX:
                RapporServiceBridge.sampleDomainAndRegistryFromURL(
                        "NTP.ExplicitUserAction.PageNavigation.OmniboxNonSearch", destinationUrl);
                return;
            case RAPPOR_ACTION_VISITED_SUGGESTED_TILE:
                RapporServiceBridge.sampleDomainAndRegistryFromURL(
                        "NTP.ExplicitUserAction.PageNavigation.NTPTileClick", destinationUrl);
                return;
            default:
                return;
        }
    }

    /**
     * Records important events related to snippets.
     * @param action action key, one of {@link SnippetsAction}'s values.
     */
    public static void recordSnippetAction(@SnippetsAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "NewTabPage.Snippets.Interactions", action, NUM_SNIPPETS_ACTIONS);
    }

    /**
     * Record a NTP impression (even potential ones to make informed product decisions).
     * @param impressionType Type of the impression from NewTabPageUma.java
     */
    public static void recordNTPImpression(int impressionType) {
        assert impressionType >= 0;
        assert impressionType < NUM_NTP_IMPRESSION;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.NTP.Impression", impressionType, NUM_NTP_IMPRESSION);
    }
}
