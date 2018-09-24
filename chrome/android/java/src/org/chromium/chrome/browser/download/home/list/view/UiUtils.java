// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.view;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.DrawableWrapper;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.RotateDrawable;
import android.graphics.drawable.ScaleDrawable;
import android.os.Build;
import android.support.annotation.DrawableRes;
import android.support.annotation.Nullable;
import android.support.annotation.StyleableRes;
import android.support.v7.content.res.AppCompatResources;

import org.chromium.base.Callback;

/** A set of helper methods to make interacting with the Android UI easier. */
public final class UiUtils {
    private UiUtils() {}

    /**
     * Loads a {@link Drawable} from an attribute.  Uses {@link AppCompatResources} to support all
     * modern {@link Drawable} types.
     * @return A new {@link Drawable} or {@code null} if the attribute wasn't set.
     */
    public static @Nullable Drawable getDrawable(
            Context context, @Nullable TypedArray attrs, @StyleableRes int attrId) {
        if (attrs == null) return null;

        @DrawableRes
        int resId = attrs.getResourceId(attrId, -1);
        if (resId == -1) return null;
        return UiUtils.getDrawable(context, resId);
    }

    /**
     * Loads a {@link Drawable} from a resource Id.  Uses {@link AppCompatResources} to support all
     * modern {@link Drawable} types.
     * @return A new {@link Drawable}.
     */
    public static Drawable getDrawable(Context context, @DrawableRes int resId) {
        return AppCompatResources.getDrawable(context, resId);
    }

    /**
     * Wraps {@code drawable} in a {@link AutoAnimatorDrawable}.
     * @return A wrapped {@code Drawable} or {@code null} if {@code drawable} is null.
     */
    public static @Nullable Drawable autoAnimateDrawable(@Nullable Drawable drawable) {
        return drawable == null ? null : new AutoAnimatorDrawable(drawable);
    }

    /**
     * Recursively searches {@code drawable} for all {@link Animatable} instances and starts them.
     * @param drawable The {@link Drawable} to start animating.
     */
    public static void startAnimatedDrawables(@Nullable Drawable drawable) {
        animatedDrawableHelper(drawable, animatable -> animatable.start());
    }

    /**
     * Recursively searches {@code drawable} for all {@link Animatable} instances and stops them.
     * @param drawable The {@link Drawable} to stop animating.
     */
    public static void stopAnimatedDrawables(@Nullable Drawable drawable) {
        animatedDrawableHelper(drawable, animatable -> animatable.stop());
    }

    @TargetApi(Build.VERSION_CODES.KITKAT)
    private static void animatedDrawableHelper(
            @Nullable Drawable drawable, Callback<Animatable> consumer) {
        if (drawable == null) return;

        if (drawable instanceof Animatable) {
            consumer.onResult((Animatable) drawable);

            // Assume Animatable drawables can handle animating their own internals/sub drawables.
            return;
        }

        if (drawable != drawable.getCurrent()) {
            // Check obvious cases where the current drawable isn't actually being shown.  This
            // should support all {@link DrawableContainer} instances.
            UiUtils.animatedDrawableHelper(drawable.getCurrent(), consumer);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && drawable instanceof DrawableWrapper) {
            // Support all modern versions of drawables that wrap other ones.  This won't cover old
            // versions of Android (see below for other if/else blocks).
            animatedDrawableHelper(((DrawableWrapper) drawable).getDrawable(), consumer);
        } else if (drawable instanceof LayerDrawable) {
            LayerDrawable layerDrawable = (LayerDrawable) drawable;
            for (int i = 0; i < layerDrawable.getNumberOfLayers(); i++) {
                animatedDrawableHelper(layerDrawable.getDrawable(i), consumer);
            }
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT
                && drawable instanceof InsetDrawable) {
            // Support legacy versions of InsetDrawable.
            animatedDrawableHelper(((InsetDrawable) drawable).getDrawable(), consumer);
        } else if (drawable instanceof RotateDrawable) {
            // Support legacy versions of RotateDrawable.
            animatedDrawableHelper(((RotateDrawable) drawable).getDrawable(), consumer);
        } else if (drawable instanceof ScaleDrawable) {
            // Support legacy versions of ScaleDrawable.
            animatedDrawableHelper(((ScaleDrawable) drawable).getDrawable(), consumer);
        }
    }
}