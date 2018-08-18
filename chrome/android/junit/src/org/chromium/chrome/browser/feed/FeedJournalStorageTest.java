// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.support.test.filters.SmallTest;

import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.host.storage.CommitResult;
import com.google.android.libraries.feed.host.storage.JournalMutation;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for {@link FeedJournalStorage}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedJournalStorageTest {
    public static final String JOURNAL_KEY1 = "JOURNAL_KEY_1";
    public static final String JOURNAL_KEY2 = "JOURNAL_KEY_2";
    public static final String JOURNAL_KEY3 = "JOURNAL_KEY_3";
    public static final String JOURNAL_DATA1 = "JOURNAL_DATA_1";
    public static final String JOURNAL_DATA2 = "JOURNAL_DATA_2";
    public static final String JOURNAL_DATA3 = "JOURNAL_DATA_3";

    @Mock
    private FeedJournalBridge mBridge;
    @Mock
    private Consumer<CommitResult> mCommitResultConsumer;
    @Mock
    private Consumer<Result<Boolean>> mBooleanConsumer;
    @Mock
            private Consumer < Result < List<byte[]>>> mListOfByteArrayConsumer;
    @Mock
            private Consumer < Result < List<String>>> mListOfStringConsumer;
    @Mock
    private Profile mProfile;
    @Captor
    private ArgumentCaptor<CommitResult> mCommitResultCaptor;
    @Captor
    private ArgumentCaptor<Result<Boolean>> mBooleanCaptor;
    @Captor
            private ArgumentCaptor < Result < List<String>>> mListOfStringCaptor;
    @Captor
            private ArgumentCaptor < Result < List<byte[]>>> mListOfByteArrayCaptor;
    @Captor
    private ArgumentCaptor<String> mStringArgument;
    @Captor
    private ArgumentCaptor<Callback<Boolean>> mBooleanCallbackArgument;
    @Captor
            private ArgumentCaptor < Callback < List<String>>> mListOfStringCallbackArgument;
    @Captor
    private ArgumentCaptor<JournalMutation> mJournalMutationArgument;

    private FeedJournalStorage mJournalStorage;

    private Answer<Void> createStringListAnswer(List<String> stringList) {
        return new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                mListOfStringCallbackArgument.getValue().onResult(stringList);
                return null;
            }
        };
    }

    private Answer<Void> createBooleanAnswer(Boolean bool) {
        return new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                mBooleanCallbackArgument.getValue().onResult(bool);
                return null;
            }
        };
    }

    private void verifyListOfBytesResult(
            List<String> expectedList, boolean expectedBoolean, Result<List<byte[]>> actualResult) {
        assertEquals(expectedBoolean, actualResult.isSuccessful());
        if (!expectedBoolean) return;

        List<byte[]> actualList = actualResult.getValue();
        assertEquals(expectedList.size(), actualList.size());
        for (byte[] actualString : actualList) {
            assertTrue(expectedList.contains(new String(actualString)));
        }
    }

    private void verifyListOfStringResult(
            List<String> expectedList, boolean expectedBoolean, Result<List<String>> actualResult) {
        assertEquals(expectedBoolean, actualResult.isSuccessful());
        if (!expectedBoolean) return;

        List<String> actualList = actualResult.getValue();
        assertEquals(expectedList.size(), actualList.size());
        for (String expectedString : expectedList) {
            assertTrue(actualList.contains(expectedString));
        }
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        doNothing().when(mBridge).init(eq(mProfile));
        mBridge.init(mProfile);
        mJournalStorage = new FeedJournalStorage(mBridge);
    }

    @Test
    @SmallTest
    public void readTest() {
        List<String> answerStrings = Arrays.asList(JOURNAL_DATA1, JOURNAL_DATA2, JOURNAL_DATA3);
        Answer<Void> answer = createStringListAnswer(answerStrings);
        doAnswer(answer).when(mBridge).loadJournal(
                mStringArgument.capture(), mListOfStringCallbackArgument.capture());

        mJournalStorage.read(JOURNAL_KEY1, mListOfByteArrayConsumer);
        verify(mBridge, times(1))
                .loadJournal(eq(JOURNAL_KEY1), mListOfStringCallbackArgument.capture());
        verify(mListOfByteArrayConsumer, times(1)).accept(mListOfByteArrayCaptor.capture());
        verifyListOfBytesResult(answerStrings, true, mListOfByteArrayCaptor.getValue());
    }

    @Test
    @SmallTest
    public void existsTest() {
        Answer<Void> answer = createBooleanAnswer(true);
        doAnswer(answer).when(mBridge).doesJournalExist(
                mStringArgument.capture(), mBooleanCallbackArgument.capture());

        mJournalStorage.exists(JOURNAL_KEY1, mBooleanConsumer);
        verify(mBridge, times(1))
                .doesJournalExist(eq(JOURNAL_KEY1), mBooleanCallbackArgument.capture());
        verify(mBooleanConsumer, times(1)).accept(mBooleanCaptor.capture());
        assertTrue(mBooleanCaptor.getValue().getValue());
    }

    @Test
    @SmallTest
    public void getAllJournalsTest() {
        List<String> answerStrings = Arrays.asList(JOURNAL_KEY1, JOURNAL_KEY2, JOURNAL_KEY3);
        Answer<Void> answer = createStringListAnswer(answerStrings);
        doAnswer(answer).when(mBridge).loadAllJournalKeys(mListOfStringCallbackArgument.capture());

        mJournalStorage.getAllJournals(mListOfStringConsumer);
        verify(mBridge, times(1)).loadAllJournalKeys(mListOfStringCallbackArgument.capture());
        verify(mListOfStringConsumer, times(1)).accept(mListOfStringCaptor.capture());
        verifyListOfStringResult(answerStrings, true, mListOfStringCaptor.getValue());
    }

    @Test
    @SmallTest
    public void deleteAllTest() {
        Answer<Void> answer = createBooleanAnswer(true);
        doAnswer(answer).when(mBridge).deleteAllJournals(mBooleanCallbackArgument.capture());

        mJournalStorage.deleteAll(mCommitResultConsumer);
        verify(mBridge, times(1)).deleteAllJournals(mBooleanCallbackArgument.capture());
        verify(mCommitResultConsumer, times(1)).accept(mCommitResultCaptor.capture());
        CommitResult commitResult = mCommitResultCaptor.getValue();
        assertEquals(CommitResult.SUCCESS, commitResult);
    }

    @Test
    @SmallTest
    public void commitTest() {
        Answer<Void> answerCommitJournal = createBooleanAnswer(true);
        doAnswer(answerCommitJournal)
                .when(mBridge)
                .commitJournalMutation(
                        mJournalMutationArgument.capture(), mBooleanCallbackArgument.capture());

        mJournalStorage.commit(new JournalMutation.Builder(JOURNAL_KEY1)
                                       .append(JOURNAL_DATA1.getBytes())
                                       .copy(JOURNAL_KEY2)
                                       .delete()
                                       .build(),
                mCommitResultConsumer);
        verify(mBridge, times(1))
                .commitJournalMutation(
                        mJournalMutationArgument.capture(), mBooleanCallbackArgument.capture());

        verify(mCommitResultConsumer, times(1)).accept(mCommitResultCaptor.capture());
        CommitResult commitResult = mCommitResultCaptor.getValue();
        assertEquals(CommitResult.SUCCESS, commitResult);
    }
}
