// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;


import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

/**
 * Robolectric tests for {@link BookmarkUtils}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, application = BaseChromiumApplication.class)
public class BookmarkUtilsTest {
    @Mock private Context mContext;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private WebContents mWebContents;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @Feature({"Bookmark"})
    public void testStartEditActivityWithoutWebContents() {
        BookmarkId bookmarkId = new BookmarkId(12345L, BookmarkType.NORMAL);
        BookmarkUtils.startEditActivity(mContext, bookmarkId, null /* webContents */);
        ArgumentCaptor<Intent> intentArgumentCaptor = ArgumentCaptor.forClass(Intent.class);

        verify(mContext).startActivity(intentArgumentCaptor.capture());

        // Verify that the intent doesn't contain the WEB_CONTENTS extra.
        assertFalse(intentArgumentCaptor.getValue()
                .hasExtra(BookmarkEditActivity.INTENT_WEB_CONTENTS));
    }

    @Test
    @Feature({"Bookmark"})
    public void testStartEditActivityWithWebContents() {
        BookmarkId bookmarkId = new BookmarkId(12345L, BookmarkType.NORMAL);
        BookmarkUtils.startEditActivity(mContext, bookmarkId, mWebContents);
        ArgumentCaptor<Intent> intentArgumentCaptor = ArgumentCaptor.forClass(Intent.class);

        verify(mContext).startActivity(intentArgumentCaptor.capture());

        // Verify that the intent contains the right WEB_CONTENTS extra.
        assertEquals(mWebContents, intentArgumentCaptor.getValue()
                .getParcelableExtra(BookmarkEditActivity.INTENT_WEB_CONTENTS));
    }

    @Test
    @Feature({"Bookmark"})
    public void testSaveBookmarkOfflineNoBookmark() {
        ApplicationStatus.initialize((BaseChromiumApplication) Robolectric.application);
        Tab tab = new Tab(0, true, null);
        BookmarkModel model = mock(BookmarkModel.class);
        SnackbarManager snackbarManager = mock(SnackbarManager.class);
        Activity activity = mock(Activity.class);

        when(model.doesBookmarkExist(new BookmarkId(12345L, BookmarkType.NORMAL)))
                .thenReturn(false);
        BookmarkUtils.saveBookmarkOffline(12345L, model, tab, snackbarManager, activity);
        verify(model).doesBookmarkExist(new BookmarkId(12345L, BookmarkType.NORMAL));

        verifyNoMoreInteractions(model);
        verifyNoMoreInteractions(snackbarManager);
        verifyNoMoreInteractions(activity);
    }

    @Test
    @Feature({"Bookmark"})
    public void testSaveBookmarkOfflineSadTab() {
        ApplicationStatus.initialize((BaseChromiumApplication) Robolectric.application);
        Tab tab = new Tab(0, true, null);
        BookmarkModel model = mock(BookmarkModel.class);
        SnackbarManager snackbarManager = mock(SnackbarManager.class);
        Activity activity = mock(Activity.class);

        tab.setIsShowingErrorPage(true);

        when(model.doesBookmarkExist(new BookmarkId(12345L, BookmarkType.NORMAL))).thenReturn(true);
        BookmarkUtils.saveBookmarkOffline(12345L, model, tab, snackbarManager, activity);
        verify(model).doesBookmarkExist(new BookmarkId(12345L, BookmarkType.NORMAL));

        verifyNoMoreInteractions(model);
        verifyNoMoreInteractions(snackbarManager);
        verifyNoMoreInteractions(activity);
    }

    @Test
    @Feature({"Bookmark"})
    public void testSaveBookmarkOffline() {
        ApplicationStatus.initialize((BaseChromiumApplication) Robolectric.application);
        Tab tab = new Tab(0, true, null);
        BookmarkModel model = mock(BookmarkModel.class);
        SnackbarManager snackbarManager = mock(SnackbarManager.class);
        Activity activity = mock(Activity.class);
        BookmarkId id = new BookmarkId(12345L, BookmarkType.NORMAL);
        when(model.doesBookmarkExist(id)).thenReturn(true);
        doNothing().when(model).saveOfflinePage(eq(id), any(WebContents.class),
                any(BookmarkModel.AddBookmarkCallback.class));
        BookmarkUtils.saveBookmarkOffline(12345L, model, tab, snackbarManager, activity);
        verify(model).doesBookmarkExist(id);
        verify(model).saveOfflinePage(eq(id), any(WebContents.class),
                any(BookmarkModel.AddBookmarkCallback.class));

        verifyNoMoreInteractions(model);
        verifyNoMoreInteractions(snackbarManager);
        verifyNoMoreInteractions(activity);
    }
}
