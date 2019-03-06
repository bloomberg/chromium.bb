// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * A footer to show some text and a link to learn more.
 */
public class Footer extends OptionalLeaf {

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
    public String describeForTesting() {
        return "FOOTER";
    }

    public void setVisible(boolean visible) {
        setVisibilityInternal(visible);
    }

    /**
     * The {@code ViewHolder} for the {@link Footer}.
     */
    public static class ViewHolder extends NewTabPageViewHolder {
        public ViewHolder(ViewGroup root, final SuggestionsNavigationDelegate navigationDelegate) {
            super(LayoutInflater.from(root.getContext())
                            .inflate(R.layout.new_tab_page_footer, root, false));

            NoUnderlineClickableSpan link = new NoUnderlineClickableSpan(
                    root.getResources(), (view) -> navigationDelegate.navigateToHelpPage());

            TextView textView = (TextView) itemView.findViewById(R.id.text);
            textView.setText(SpanApplier.applySpans(
                    root.getResources().getString(R.string.ntp_learn_more_about_suggested_content),
                    new SpanApplier.SpanInfo("<link>", "</link>", link)));
            textView.setMovementMethod(LinkMovementMethod.getInstance());
        }
    }
}
