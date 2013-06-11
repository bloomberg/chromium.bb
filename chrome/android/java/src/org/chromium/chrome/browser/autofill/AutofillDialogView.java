// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.chrome.browser.autofill.AutofillDialogConstants.NUM_SECTIONS;
import static org.chromium.chrome.browser.autofill.AutofillDialogConstants.SECTION_BILLING;
import static org.chromium.chrome.browser.autofill.AutofillDialogConstants.SECTION_CC;
import static org.chromium.chrome.browser.autofill.AutofillDialogConstants.SECTION_CC_BILLING;
import static org.chromium.chrome.browser.autofill.AutofillDialogConstants.SECTION_EMAIL;
import static org.chromium.chrome.browser.autofill.AutofillDialogConstants.SECTION_SHIPPING;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.animation.PropertyValuesHolder;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Property;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.view.animation.AnimationSet;
import android.view.animation.ScaleAnimation;
import android.view.animation.TranslateAnimation;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * This is the parent layout that contains the layouts for different states of
 * autofill dialog. In principle it shouldn't contain any logic related with the
 * actual workflow, but rather respond to any UI update messages coming to it
 * from the AutofillDialog.
 */
public class AutofillDialogView extends FrameLayout {
    private static final int ANIMATION_DURATION_MS = 600;
    static final int INVALID_LAYOUT = -1;
    static final int LAYOUT_EDITING_EMAIL = 0;
    static final int LAYOUT_EDITING_SHIPPING = 1;
    static final int LAYOUT_EDITING_CC = 2;
    static final int LAYOUT_EDITING_BILLING = 3;
    static final int LAYOUT_EDITING_CC_BILLING = 4;
    static final int LAYOUT_FETCHING = 5;
    static final int LAYOUT_STEADY = 6;
    private final Runnable mDismissSteadyLayoutRunnable = new Runnable() {
        @Override
        public void run() {
            mSteadyLayout.setVisibility(GONE);
        }
    };
    private final Property<AutofillDialogView, Float> mContentPropertyHeight =
            new Property<AutofillDialogView, Float>(Float.class, "") {
        @Override
        public Float get(AutofillDialogView view) {
            return view.getVisibleContentHeight();
        }

        @Override
        public void set(AutofillDialogView view, Float height) {
            view.setVisibleContentHeight(height);
        }
    };
    private AutofillDialog mDialog;
    private Spinner[] mSpinners = new Spinner[NUM_SECTIONS];
    private AutofillDialogMenuAdapter[] mAdapters = new AutofillDialogMenuAdapter[NUM_SECTIONS];
    private ViewGroup mSteadyLayout;
    private ViewGroup[] mEditLayouts = new ViewGroup[NUM_SECTIONS];
    private int mCurrentLayout = -1;
    private OnItemSelectedListener mOnItemSelectedListener;
    private OnItemEditButtonClickedListener mOnItemEditButtonClickedListener;
    private View mTitle;
    private View mContent;
    private View mFooter;
    private boolean mTransitionAnimationPlaying;
    private float mVisibleContentHeight;
    private ObjectAnimator mCurrentHeightAnimator;
    private AnimationSet mCurrentLayoutAnimation;
    private Drawable mBackground = new ColorDrawable(Color.LTGRAY);
    private boolean mAnimateOnNextLayoutChange = false;
    private int mCurrentOrientation;
    private int mFrameHeight;
    private int mFrameWidth;

    /**
     * Interface definition for a callback to be invoked when an "Edit" button
     * in the AutofillDialogMenuAdapter is clicked.
     */
    public interface OnItemEditButtonClickedListener {
        /**
         * Callback method to be invoked when an "Edit" button has been clicked.
         * @param section The dialog section associated with the adapter.
         * @param position The position of the view in the adapter
         */
        void onItemEditButtonClicked(int section, int position);
    }

    public AutofillDialogView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setWillNotDraw(false);
    }

    @Override
    protected void onFinishInflate () {
        mSteadyLayout = (ViewGroup) findViewById(R.id.general_layout);
        mTitle = findViewById(R.id.title);
        mContent = findViewById(R.id.content);
        mFooter = findViewById(R.id.footer);

        for (int i = 0; i < AutofillDialogConstants.NUM_SECTIONS; i++) {
            mEditLayouts[i] = (ViewGroup) findViewById(
                    AutofillDialogUtils.getLayoutIDForSection(i));
            int id = AutofillDialogUtils.getSpinnerIDForSection(i);
            mSpinners[i] = (Spinner) findViewById(id);
        }

        int spec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        for (int i = 0; i < NUM_SECTIONS; i++) {
            (mEditLayouts[i]).measure(spec, spec);
        }
        findViewById(R.id.loading_icon).measure(spec, spec);

        mContent.addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (mAnimateOnNextLayoutChange && mCurrentHeightAnimator != null
                        && !mCurrentHeightAnimator.isStarted()) {
                    mCurrentHeightAnimator.start();
                    mAnimateOnNextLayoutChange = false;
                    if (mCurrentLayoutAnimation != null && !mCurrentLayoutAnimation.hasStarted()) {
                        mCurrentLayoutAnimation.start();
                    }
                }
            }
        });

        mVisibleContentHeight = getAdjustedContentLayoutHeight(LAYOUT_FETCHING, null);
        changeLayoutTo(LAYOUT_FETCHING);
    }

    /**
     * Sets the controller dialog for this content view and initializes the view.
     * @param dialog The autofill Dialog that should be set as the controller.
     */
    public void initialize(AutofillDialog dialog) {
        mDialog = dialog;

        mDialog.getWindow().getDecorView().setBackgroundResource(0);

        FrameLayout.LayoutParams layoutParams =
                (android.widget.FrameLayout.LayoutParams) getLayoutParams();

        mCurrentOrientation = getResources().getConfiguration().orientation;
        mFrameHeight = getResources().getDimensionPixelSize(R.dimen.autofill_dialog_max_height);
        layoutParams.height = mFrameHeight;
        mFrameWidth = getResources().getDimensionPixelSize(R.dimen.autofill_dialog_width);
        layoutParams.width = mFrameWidth;
        requestLayout();
    }

    @Override
    protected void onConfigurationChanged (Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (mCurrentOrientation == newConfig.orientation) return;

        mCurrentOrientation = newConfig.orientation;
        FrameLayout.LayoutParams layoutParams =
                (android.widget.FrameLayout.LayoutParams) getLayoutParams();
        mFrameHeight = getResources().getDimensionPixelSize(R.dimen.autofill_dialog_max_height);
        layoutParams.height = mFrameHeight;
        requestLayout();

    }

    @Override
    protected boolean drawChild(Canvas canvas, View child, long drawingTime) {
        if (!mTransitionAnimationPlaying || child.getId() != R.id.content) {
            return super.drawChild(canvas, child, drawingTime);
        }

        float top = mContent.getTop() + getTranslationForHiddenContentHeight();
        float bottom = mContent.getBottom() - getTranslationForHiddenContentHeight();

        canvas.save();
        canvas.clipRect(getLeft(), top, getRight(), bottom);
        boolean result = super.drawChild(canvas, child, drawingTime);
        canvas.restore();
        return result;
    }

    @Override
    public void onDraw(Canvas canvas) {
        if (mTransitionAnimationPlaying) {
            mTitle.setTranslationY(getTranslationForHiddenContentHeight());
            mContent.setTranslationY(getTranslationForHiddenContentHeight());
            mFooter.setTranslationY(-getTranslationForHiddenContentHeight());
        }

        canvas.save();
        mBackground.setBounds(getLeft() ,(int) (mTitle.getTop() + mTitle.getTranslationY()),
                getRight(),(int) (mFooter.getBottom() + mFooter.getTranslationY()));
        mBackground.draw(canvas);
        canvas.restore();

        super.onDraw(canvas);
    }

    /**
     * Prompts the content view to create the adapters for each section. This is
     * separated to be able to control the timing in a flexible manner.
     */
    public void createAdapters() {
        for (int i = 0; i < AutofillDialogConstants.NUM_SECTIONS; i++) {
            AutofillDialogMenuAdapter adapter =
                    new AutofillDialogMenuAdapter(i, getContext(),
                            new ArrayList<AutofillDialogMenuItem>());
            adapter.setOnItemEditButtonClickedListener(mOnItemEditButtonClickedListener);
            mAdapters[i] = adapter;
            mSpinners[i].setAdapter(adapter);
        }

        initializeSpinner(SECTION_SHIPPING, AutofillDialogConstants.ADDRESS_HOME_COUNTRY);
        initializeSpinner(SECTION_BILLING, AutofillDialogConstants.ADDRESS_BILLING_COUNTRY);
        initializeSpinner(SECTION_CC, AutofillDialogConstants.CREDIT_CARD_EXP_MONTH);
        initializeSpinner(SECTION_CC, AutofillDialogConstants.CREDIT_CARD_EXP_4_DIGIT_YEAR);
    }

    private void initializeSpinner(int section, int field) {
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(getContext(),
                R.layout.autofill_editing_spinner_item);
        adapter.addAll(Arrays.asList(mDialog.getListForField(field)));
        ((Spinner) findViewById(AutofillDialogUtils
                .getViewIDForField(section, field))).setAdapter(adapter);
    }

    /**
     * Initializes the labels for each section.
     * @param labels The label strings to be used for each section.
     */
    public void initializeLabelsForEachSection(String[] labels) {
        TextView labelView;
        for (int i = 0; i < labels.length && i < NUM_SECTIONS; i++) {
            labelView = (TextView) findViewById(AutofillDialogUtils.getLabelIDForSection(i));
            labelView.setText(labels[i]);
        }
    }

    private int getMeasuredTitleHeight() {
        return mTitle.getMeasuredHeight();
    }

    private int getMeasuredFooterHeight() {
        return mFooter.getMeasuredHeight();
    }

    private float getTranslationForHiddenContentHeight() {
        return (mContent.getMeasuredHeight() - mVisibleContentHeight) / 2;
    }

    private int getAdjustedContentLayoutHeight(int mode, View layout) {
        assert mode != INVALID_LAYOUT;
        int measuredContentHeight;
        if (mode == LAYOUT_STEADY) {
            measuredContentHeight = mSteadyLayout.getMeasuredHeight();
        } else if (mode == LAYOUT_FETCHING) {
            measuredContentHeight = findViewById(R.id.loading_icon).getMeasuredHeight();
        } else {
            if (mode == LAYOUT_EDITING_CC_BILLING) {
                // For CC_BILLING we have to measure again since it contains both CC and
                // BILLING also. The last measure may be for just one of them.
                // TODO(yusufo): Do this if the mode that was measured before is wrong.
                int widthSpec = MeasureSpec.makeMeasureSpec(getWidth(), MeasureSpec.EXACTLY);
                int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
                layout.measure(widthSpec, heightSpec);
            }
            measuredContentHeight = layout.getMeasuredHeight();
        }
        return Math.min(measuredContentHeight,
                getMeasuredHeight() - getMeasuredTitleHeight() - getMeasuredFooterHeight());
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // When the new layout height is higher than the old one in a transition, we end up with a
        // a nonzero translation value at the end of the animation, since we set translations only
        // while we are animating we set them back to zero here on a measure.
        mTitle.setTranslationY(0);
        mContent.setTranslationY(0);
        mFooter.setTranslationY(0);

        Rect outRect = new Rect();
        mDialog.getWindow().getDecorView().getWindowVisibleDisplayFrame(outRect);
        Resources resources = getResources();
        mFrameHeight = Math.min(
                resources.getDimensionPixelSize(R.dimen.autofill_dialog_max_height),
                        outRect.height() - 2 * resources.getDimensionPixelSize(
                                R.dimen.autofill_dialog_vertical_margin));

        int widthSpec = MeasureSpec.makeMeasureSpec(mFrameWidth, MeasureSpec.EXACTLY);
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        measureChild(mTitle, widthSpec, heightSpec);
        measureChild(mFooter, widthSpec, heightSpec);
        heightSpec = MeasureSpec.makeMeasureSpec(
                mFrameHeight - getMeasuredTitleHeight() - getMeasuredFooterHeight(),
                MeasureSpec.AT_MOST);
        mContent.measure(widthSpec, heightSpec);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            for (int i = 0; i < NUM_SECTIONS; i++) {
                mSpinners[i].setDropDownWidth(mSpinners[i].getMeasuredWidth());
                mSpinners[i].setDropDownVerticalOffset(-mSpinners[i].getMeasuredHeight());
            }
        }
        setMeasuredDimension(mFrameWidth, mFrameHeight);
    }

    @Override
    protected void onLayout (boolean changed, int left, int top, int right, int bottom) {
        int totalHeight = getMeasuredTitleHeight()
                + getMeasuredFooterHeight() + mContent.getMeasuredHeight();
        int verticalSpacing = (getMeasuredHeight() - totalHeight) / 2 ;
        mTitle.layout(left, verticalSpacing, right, getMeasuredTitleHeight() + verticalSpacing);
        mContent.layout(left, mTitle.getBottom(), right,
                mTitle.getBottom() + mContent.getMeasuredHeight());
        mFooter.layout(left, mContent.getBottom(), right, bottom - verticalSpacing);
    }

    /**
     * Set the listener for all the dropdown members in the layout.
     * @param listener The listener object to attach to the dropdowns.
     */
    public void setOnItemSelectedListener(OnItemSelectedListener listener) {
        mOnItemSelectedListener = listener;
        for (int i = 0; i < NUM_SECTIONS; i++) mSpinners[i].setOnItemSelectedListener(listener);
    }

    /**
     * Set the listener for all the dropdown members in the layout.
     * @param listener The listener object to attach to the dropdowns.
     */
    public void setOnItemEditButtonClickedListener(OnItemEditButtonClickedListener listener) {
        mOnItemEditButtonClickedListener = listener;
        for (int i = 0; i < NUM_SECTIONS; i++) {
            if (mAdapters[i] != null) {
                mAdapters[i].setOnItemEditButtonClickedListener(listener);
            }
        }
    }

    /**
     * @return Whether the current layout is one of the editing layouts.
     */
    public boolean isInEditingMode() {
        return mCurrentLayout != INVALID_LAYOUT
                && mCurrentLayout != LAYOUT_STEADY
                && mCurrentLayout != LAYOUT_FETCHING;
    }

    /**
     * @return The current section if we are in editing mode, INVALID_SECTION otherwise.
     */
    public int getCurrentSection() {
        return getSectionForLayoutMode(mCurrentLayout);
    }

    /**
     * Updates the legal documents footer.
     * @param legalDocumentsText The text to show, or a null/empty string.
     */
    public void updateLegalDocumentsText(String legalDocumentsText) {
        TextView legalView = (TextView) findViewById(R.id.terms_info);
        if (TextUtils.isEmpty(legalDocumentsText)) {
            legalView.setVisibility(View.GONE);
            return;
        }

        legalView.setText(legalDocumentsText);
        legalView.setVisibility(View.VISIBLE);
    }

    /**
     * Updates a dropdown with the given items and adds default items to the end.
     * @param section The dialog section.
     * @param suggestionText The suggestion text.
     * @param suggestionIcon The suggestion icon.
     * @param suggestionTextExtra The suggestion text extra.
     * @param suggestionIconExtra The suggestion icon extra.
     * @param items The {@link AutofillDialogMenuItem} array to update the dropdown with.
     * @param selectedMenuItem The index of the selected menu item, or -1.
     */
    public void updateMenuItemsForSection(int section,
            String suggestionText, Bitmap suggestionIcon,
            String suggestionTextExtra, Bitmap suggestionIconExtra,
            List<AutofillDialogMenuItem> items, final int selectedMenuItem) {
        final Spinner spinner = mSpinners[section];
        // Set the listener to null and reset it after updating the menu items to avoid getting an
        // onItemSelected call when the first item is selected after updating the items.
        spinner.setOnItemSelectedListener(null);
        AutofillDialogMenuAdapter adapter = mAdapters[section];
        adapter.clear();
        adapter.setSuggestionExtra(suggestionTextExtra,
                createFieldIconDrawable(suggestionIconExtra));
        adapter.addAll(items);
        spinner.setSelection(selectedMenuItem);
        spinner.post(new Runnable() {
            @Override
            public void run() {
                spinner.setOnItemSelectedListener(mOnItemSelectedListener);
            }
        });
    }

    /**
     * Updates a dropdown selection.
     * @param section The dialog section.
     * @param selectedMenuItem The index of the selected menu item, or -1.
     */
    public void updateMenuSelectionForSection(int section, int selectedMenuItem) {
        final Spinner spinner = mSpinners[section];
        spinner.setSelection(selectedMenuItem);
    }

    /**
     * Transitions the layout shown to a given layout.
     * @param mode The layout mode to transition to.
     */
    public void changeLayoutTo(int mode) {
        assert(mode != INVALID_LAYOUT);
        if (mode == mCurrentLayout || mode == INVALID_LAYOUT) return;

        mCurrentLayout = mode;
        removeCallbacks(mDismissSteadyLayoutRunnable);
        setVisibilityForEditLayouts(false);
        if (mode == LAYOUT_FETCHING) {
            mSteadyLayout.setVisibility(GONE);
            findViewById(R.id.loading_icon).setVisibility(VISIBLE);
            return;
        }
        findViewById(R.id.loading_icon).setVisibility(GONE);
        mSteadyLayout.setVisibility(VISIBLE);
        if (mode == LAYOUT_STEADY) return;

        View centeredLayout = mEditLayouts[getSectionForLayoutMode(mode)];

        if (mode == LAYOUT_EDITING_CC_BILLING) {
            mEditLayouts[SECTION_CC].setVisibility(VISIBLE);
            mEditLayouts[SECTION_BILLING].setVisibility(VISIBLE);
        } else if (mode == LAYOUT_EDITING_CC || mode == LAYOUT_EDITING_BILLING) {
            mEditLayouts[SECTION_CC_BILLING].setVisibility(VISIBLE);
        }
        centeredLayout.setVisibility(VISIBLE);
        final float fromHeight = mSteadyLayout.getMeasuredHeight();
        final float toHeight = getAdjustedContentLayoutHeight(mode, centeredLayout);
        PropertyValuesHolder pvhVisibleHeight = PropertyValuesHolder.ofFloat(
                mContentPropertyHeight, fromHeight, toHeight);
        mCurrentHeightAnimator =
                ObjectAnimator.ofPropertyValuesHolder(this, pvhVisibleHeight);
        mCurrentHeightAnimator.setDuration(ANIMATION_DURATION_MS);
        mCurrentHeightAnimator.addUpdateListener(new AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animator) {
                invalidate();
            }
        });
        mCurrentHeightAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animator) {
                mTransitionAnimationPlaying = true;
            }

            @Override
            public void onAnimationEnd(Animator animator) {
                mTransitionAnimationPlaying = false;
                mVisibleContentHeight = toHeight;
            }
        });
        mCurrentLayoutAnimation = addTranslateAnimations(mode);
        mCurrentLayoutAnimation.addAnimation(addAppearAnimationForEditLayout(mode, centeredLayout));
        mAnimateOnNextLayoutChange = true;

        postDelayed(mDismissSteadyLayoutRunnable, ANIMATION_DURATION_MS);
        mCurrentLayout = mode;
    }

    private void setVisibilityForEditLayouts(boolean visible) {
        int visibility = visible ? VISIBLE : GONE;
        for (int i = 0; i < NUM_SECTIONS; i++) {
            if (mEditLayouts[i] != null) mEditLayouts[i].setVisibility(visibility);
        }
    }

    /**
     * Sets the visibility for all items related with the given section.
     * @param section The section that will change visibility.
     * @param visible Whether the section should be visible.
     */
    public void setVisibilityForSection(int section, boolean visible) {
        int visibility = visible ? VISIBLE : GONE;
        mSpinners[section].setVisibility(visibility);
        View labelView = findViewById(AutofillDialogUtils.getLabelIDForSection(section));
        if (labelView != null) labelView.setVisibility(visibility);
    }

    /**
     * @param bitmap A bitmap.
     * @return A drawable with the dimensions of a field icon and the contents from the bitmap.
     */
    public BitmapDrawable createFieldIconDrawable(Bitmap bitmap) {
        if (bitmap == null) return null;

        Resources resources = getContext().getResources();
        BitmapDrawable drawable = new BitmapDrawable(resources, bitmap);
        int width = resources.getDimensionPixelSize(R.dimen.autofill_field_icon_width);
        int height = resources.getDimensionPixelSize(R.dimen.autofill_field_icon_height);
        drawable.setBounds(0, 0, width, height);
        return drawable;
    }

    private AnimationSet addAppearAnimationForEditLayout(int mode, View layout) {
        View centerView = mSpinners[getSectionForLayoutMode(mode)];
        float yOffset = centerView.getY() - (float) centerView.getHeight() / 2;

        TranslateAnimation toLocationAnimation = new TranslateAnimation(0, 0, yOffset, 0);
        toLocationAnimation.setDuration(ANIMATION_DURATION_MS);
        ScaleAnimation scaleAnimation = new ScaleAnimation(1, 1, 0, 1,
                Animation.RELATIVE_TO_SELF, 0, Animation.RELATIVE_TO_SELF, 0.5f);
        scaleAnimation.setDuration(ANIMATION_DURATION_MS);

        AnimationSet appearAnimation = new AnimationSet(true);
        appearAnimation.addAnimation(toLocationAnimation);
        appearAnimation.addAnimation(scaleAnimation);

        layout.setAnimation(appearAnimation);
        return appearAnimation;
    }

    private AnimationSet addTranslateAnimations(int mode) {
        float distance = getResources().
                getDimensionPixelSize(R.dimen.autofill_translation_anim_distance);
        TranslateAnimation toTopAnimation = new TranslateAnimation(0, 0, 0, -distance);
        toTopAnimation.setDuration(ANIMATION_DURATION_MS);
        TranslateAnimation toBottomAnimation = new TranslateAnimation(0, 0, 0, distance);
        toBottomAnimation.setDuration(ANIMATION_DURATION_MS);
        for (int i = 0; i < mSteadyLayout.getChildCount(); i++) {
            View currentChild = mSteadyLayout.getChildAt(i);
            if (currentChild.getVisibility() == GONE) continue;

            if (currentChild.getTop() <=
                    mSpinners[getSectionForLayoutMode(mode)].getTop()) {
                currentChild.setAnimation(toTopAnimation);
            } else {
                currentChild.setAnimation(toBottomAnimation);
            }
        }
        AnimationSet combined = new AnimationSet(true);
        combined.addAnimation(toBottomAnimation);
        combined.addAnimation(toTopAnimation);
        return combined;
    }

    private static int getSectionForLayoutMode(int mode) {
        switch (mode) {
            case LAYOUT_EDITING_EMAIL:
                return SECTION_EMAIL;
            case LAYOUT_EDITING_CC:
                return SECTION_CC;
            case LAYOUT_EDITING_BILLING:
                return SECTION_BILLING;
            case LAYOUT_EDITING_CC_BILLING:
                return SECTION_CC_BILLING;
            case LAYOUT_EDITING_SHIPPING:
                return SECTION_SHIPPING;
            default:
                assert(false);
                return AutofillDialogUtils.INVALID_SECTION;
        }
    }

    /**
     * Returns the layout mode for editing the given section.
     * @param section The section to look up.
     * @return The layout mode for editing the given section.
     */
    public static int getLayoutModeForSection(int section) {
        assert(section != AutofillDialogUtils.INVALID_SECTION);
        for (int i = 0; i < AutofillDialogConstants.NUM_SECTIONS; i++) {
            if (getSectionForLayoutMode(i) == section) return i;
        }
        assert(false);
        return INVALID_LAYOUT;
    }

    /**
     * @return The current visible content height.
     */
    public float getVisibleContentHeight() {
        return mVisibleContentHeight;
    }

    /**
     * Sets the visible content height. This value is used for setting the clipRect for the canvas
     * in layout transitions.
     * @param height The value to use for the visible content height.
     */
    public void setVisibleContentHeight(float height) {
        mVisibleContentHeight = height;
    }

    private static class AutofillDialogMenuAdapter extends ArrayAdapter<AutofillDialogMenuItem> {
        private int mSection;
        private OnItemEditButtonClickedListener mOnItemEditButtonClickedListener;
        private String mSuggestionTextExtra;
        private BitmapDrawable mSuggestionIconExtraDrawable;

        public AutofillDialogMenuAdapter(
                int section,
                Context context,
                List<AutofillDialogMenuItem> objects) {
            super(context, R.layout.autofill_menu_item, objects);
            mSection = section;
        }

        public void setOnItemEditButtonClickedListener(OnItemEditButtonClickedListener listener) {
            mOnItemEditButtonClickedListener = listener;
        }

        public void setSuggestionExtra(String suggestionTextExtra,
                BitmapDrawable suggestionIconExtraDrawable) {
            mSuggestionTextExtra = suggestionTextExtra;
            mSuggestionIconExtraDrawable = suggestionIconExtraDrawable;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            return initView(position, convertView, parent, false,
                    AutofillDialogUtils.getItemLayoutIDForSection(mSection));
        }

        @Override
        public View getDropDownView(int position, View convertView, ViewGroup parent) {
            return initView(position, convertView, parent, true, R.layout.autofill_menu_item);
        }

        private View initView(
                final int position, View convertView, final ViewGroup parent, boolean isDropDown,
                        int layoutId) {
            if (convertView == null) {
                convertView = View.inflate(getContext(), layoutId, null);
            }

            AutofillDialogMenuItem item = getItem(position);
            ImageView icon = (ImageView) convertView.findViewById(R.id.cc_icon);
            TextView line1 = (TextView) convertView.findViewById(R.id.adapter_item_line_1);
            TextView line2 = (TextView) convertView.findViewById(R.id.adapter_item_line_2);
            Button button = (Button) convertView.findViewById(R.id.adapter_item_edit_button);
            EditText extraEdit = (EditText) convertView.findViewById(R.id.cvc_challenge);

            if (icon != null) {
                if (item.mIcon != null) {
                    icon.setImageBitmap(item.mIcon);
                    icon.setVisibility(VISIBLE);
                } else {
                    icon.setImageBitmap(null);
                    icon.setVisibility(GONE);
                }
            }
            if (line1 != null) line1.setText(item.mLine1);
            if (line2 != null) {
                if (!TextUtils.isEmpty(item.mLine2)) {
                    line2.setVisibility(VISIBLE);
                    line2.setText(item.mLine2);
                } else {
                    line2.setVisibility(GONE);
                }
            }

            if (extraEdit != null) {
                if (!isDropDown && !TextUtils.isEmpty(mSuggestionTextExtra)) {
                  extraEdit.setVisibility(VISIBLE);
                  extraEdit.setHint(mSuggestionTextExtra);
                  extraEdit.setCompoundDrawables(
                          null, null, mSuggestionIconExtraDrawable, null);
                } else {
                  extraEdit.setVisibility(GONE);
                }
            }

            if (button != null) {
                if (isDropDown && item.mShowButton) {
                    button.setText(item.mButtonLabelResourceId);
                    button.setOnClickListener(new OnClickListener() {
                        // TODO(aruslan): http://crbug.com/236101.
                        @Override
                        public void onClick(View view) {
                            View root = parent.getRootView();
                            root.dispatchKeyEvent(
                                    new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK));
                            root.dispatchKeyEvent(
                                    new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK));
                            assert mOnItemEditButtonClickedListener != null;
                            if (mOnItemEditButtonClickedListener != null) {
                                mOnItemEditButtonClickedListener.onItemEditButtonClicked(
                                        mSection, position);
                            }
                        }
                    });
                    button.setVisibility(VISIBLE);
                } else {
                    button.setOnClickListener(null);
                    button.setVisibility(GONE);
                }
            }
            return convertView;
        }
    }
}
