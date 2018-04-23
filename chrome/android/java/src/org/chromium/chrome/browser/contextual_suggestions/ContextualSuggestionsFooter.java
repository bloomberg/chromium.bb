// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.Leaf;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.cards.NodeVisitor;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.ui.text.NoUnderlineClickableSpan;

/**
 * A footer to show some a "Send Feedback" link.
 * TODO(twellington): Remove when feedback link is moved into overflow menu
 * (https://crbug.com/831782).
 */
public class ContextualSuggestionsFooter extends Leaf {
    @Override
    @ItemViewType
    protected int getItemViewType() {
        return ItemViewType.FOOTER;
    }

    @Override
    protected void onBindViewHolder(NewTabPageViewHolder holder) {
        // Nothing to do (the footer view is static).
    }

    @Override
    public void visitItems(NodeVisitor visitor) {
        visitor.visitFooter();
    }

    /**
     * The {@code ViewHolder} for the {@link Footer}.
     */
    public static class ViewHolder extends NewTabPageViewHolder {
        public ViewHolder(ViewGroup root, final SuggestionsNavigationDelegate navigationDelegate) {
            super(LayoutInflater.from(root.getContext())
                            .inflate(R.layout.contextual_suggestions_footer, root, false));

            NoUnderlineClickableSpan link = new NoUnderlineClickableSpan() {
                @Override
                public void onClick(View view) {
                    navigationDelegate.showFeedback();
                }
            };

            SpannableString text = new SpannableString(
                    root.getResources().getString(R.string.sad_tab_send_feedback_label));
            text.setSpan(link, 0, text.length(), 0);

            TextView textView = (TextView) itemView.findViewById(R.id.text);
            textView.setText(text);
            textView.setMovementMethod(LinkMovementMethod.getInstance());
        }
    }
}