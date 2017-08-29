// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.os.Handler;
import android.os.LocaleList;
import android.support.annotation.IntDef;
import android.view.View.OnClickListener;
import android.view.textclassifier.TextClassification;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextSelection;

import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/**
 * Controls Smart Text selection. Talks to the Android TextClassificationManager API.
 */
public class SmartSelectionProvider {
    /**
     * The result of the text analysis.
     */
    public static class Result {
        /**
         * The number of characters that the left boundary of the original
         * selection should be moved. Negative number means moving left.
         */
        public int startAdjust;

        /**
         * The number of characters that the right boundary of the original
         * selection should be moved. Negative number means moving left.
         */
        public int endAdjust;

        /**
         * Label for the suggested menu item.
         */
        public CharSequence label;

        /**
         * Icon for the suggested menu item.
         */
        public Drawable icon;

        /**
         * Intent for the suggested menu item.
         */
        public Intent intent;

        /**
         * OnClickListener for the suggested menu item.
         */
        public OnClickListener onClickListener;

        /**
         * A helper method that returns true if the result has both visual info
         * and an action so that, for instance, one can make a new menu item.
         */
        public boolean hasNamedAction() {
            return (label != null || icon != null) && (intent != null || onClickListener != null);
        }
    }

    /**
     * The interface that returns the result of the selected text analysis.
     */
    public interface ResultCallback {
        /**
         * The result is delivered with this method.
         */
        void onClassified(Result result);
    }

    private static final String TAG = "SmartSelProvider";

    @IntDef({CLASSIFY, SUGGEST_AND_CLASSIFY})
    @Retention(RetentionPolicy.SOURCE)
    private @interface RequestType {}

    private static final int CLASSIFY = 0;
    private static final int SUGGEST_AND_CLASSIFY = 1;

    private ResultCallback mResultCallback;
    private WindowAndroid mWindowAndroid;
    private ClassificationTask mClassificationTask;
    private TextClassifier mTextClassifier;

    private Handler mHandler;
    private Runnable mFailureResponseRunnable;

    public SmartSelectionProvider(ResultCallback callback, WindowAndroid windowAndroid) {
        mResultCallback = callback;
        mWindowAndroid = windowAndroid;
        mHandler = new Handler();
        mFailureResponseRunnable = new Runnable() {
            @Override
            public void run() {
                mResultCallback.onClassified(new Result());
            }
        };
    }

    public void sendSuggestAndClassifyRequest(
            CharSequence text, int start, int end, Locale[] locales) {
        sendSmartSelectionRequest(SUGGEST_AND_CLASSIFY, text, start, end, locales);
    }

    public void sendClassifyRequest(CharSequence text, int start, int end, Locale[] locales) {
        sendSmartSelectionRequest(CLASSIFY, text, start, end, locales);
    }

    public void cancelAllRequests() {
        if (mClassificationTask != null) {
            mClassificationTask.cancel(false);
            mClassificationTask = null;
        }
    }

    public void setTextClassifier(TextClassifier textClassifier) {
        mTextClassifier = textClassifier;
    }

    // TODO(crbug.com/739746): Remove suppression when this constant is added to lint.
    @SuppressLint("WrongConstant")
    public TextClassifier getTextClassifier() {
        if (mTextClassifier != null) return mTextClassifier;

        Context context = mWindowAndroid.getContext().get();
        if (context == null) return null;

        return ((TextClassificationManager) context.getSystemService(
                        Context.TEXT_CLASSIFICATION_SERVICE))
                .getTextClassifier();
    }

    public TextClassifier getCustomTextClassifier() {
        return mTextClassifier;
    }

    private void sendSmartSelectionRequest(
            @RequestType int requestType, CharSequence text, int start, int end, Locale[] locales) {
        TextClassifier classifier = (TextClassifier) getTextClassifier();
        if (classifier == null || classifier == TextClassifier.NO_OP) {
            mHandler.post(mFailureResponseRunnable);
            return;
        }

        if (mClassificationTask != null) {
            mClassificationTask.cancel(false);
            mClassificationTask = null;
        }

        mClassificationTask =
                new ClassificationTask(classifier, requestType, text, start, end, locales);
        mClassificationTask.execute();
    }

    private class ClassificationTask extends AsyncTask<Void, Void, Result> {
        private final TextClassifier mTextClassifier;
        private final int mRequestType;
        private final CharSequence mText;
        private final int mOriginalStart;
        private final int mOriginalEnd;
        private final Locale[] mLocales;

        ClassificationTask(TextClassifier classifier, @RequestType int requestType,
                CharSequence text, int start, int end, Locale[] locales) {
            mTextClassifier = classifier;
            mRequestType = requestType;
            mText = text;
            mOriginalStart = start;
            mOriginalEnd = end;
            mLocales = locales;
        }

        @Override
        protected Result doInBackground(Void... params) {
            int start = mOriginalStart;
            int end = mOriginalEnd;

            if (mRequestType == SUGGEST_AND_CLASSIFY) {
                TextSelection suggested = mTextClassifier.suggestSelection(
                        mText, start, end, makeLocaleList(mLocales));
                start = Math.max(0, suggested.getSelectionStartIndex());
                end = Math.min(mText.length(), suggested.getSelectionEndIndex());
                if (isCancelled()) return new Result();
            }

            TextClassification tc =
                    mTextClassifier.classifyText(mText, start, end, makeLocaleList(mLocales));
            return makeResult(start, end, tc);
        }

        @SuppressLint("NewApi")
        private LocaleList makeLocaleList(Locale[] locales) {
            if (locales == null || locales.length == 0) return null;
            return new LocaleList(locales);
        }

        private Result makeResult(int start, int end, TextClassification tc) {
            Result result = new Result();

            result.startAdjust = start - mOriginalStart;
            result.endAdjust = end - mOriginalEnd;
            result.label = tc.getLabel();
            result.icon = tc.getIcon();
            result.intent = tc.getIntent();
            result.onClickListener = tc.getOnClickListener();

            return result;
        }

        @Override
        protected void onPostExecute(Result result) {
            mResultCallback.onClassified(result);
        }
    }
}
