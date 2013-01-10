// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

import android.content.Context;
import android.graphics.PixelFormat;
import android.telephony.TelephonyManager;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.WindowManager;

import org.chromium.base.CalledByNative;

/**
 * This class facilitates access to android information typically only
 * available using the Java SDK, including {@link Display} properties.
 *
 * Currently the information consists of very raw display information (height, width, DPI scale)
 * regarding the main display, and also the current telephony region.
 */
public class DeviceInfo {

  private WindowManager mWinManager;
  private TelephonyManager mTelManager;

  private DeviceInfo(Context context) {
      Context appContext = context.getApplicationContext();
      mWinManager = (WindowManager) appContext.getSystemService(Context.WINDOW_SERVICE);
      mTelManager = (TelephonyManager) appContext.getSystemService(Context.TELEPHONY_SERVICE);
  }

  @CalledByNative
  public int getHeight() {
      return getMetrics().heightPixels;
  }

  @CalledByNative
  public int getWidth() {
      return getMetrics().widthPixels;
  }

  @CalledByNative
  public int getBitsPerPixel() {
      PixelFormat info = new PixelFormat();
      PixelFormat.getPixelFormatInfo(getDisplay().getPixelFormat(), info);
      return info.bitsPerPixel;
  }

  @CalledByNative
  public int getBitsPerComponent() {
      int format = getDisplay().getPixelFormat();
      switch (format) {
      case PixelFormat.RGBA_4444:
          return 4;

      case PixelFormat.RGBA_5551:
          return 5;

      case PixelFormat.RGBA_8888:
      case PixelFormat.RGBX_8888:
      case PixelFormat.RGB_888:
          return 8;

      case PixelFormat.RGB_332:
          return 2;

      case PixelFormat.RGB_565:
          return 5;

      // Non-RGB formats.
      case PixelFormat.A_8:
      case PixelFormat.LA_88:
      case PixelFormat.L_8:
          return 0;

      // Unknown format. Use 8 as a sensible default.
      default:
          return 8;
      }
  }

  @CalledByNative
  public double getDPIScale() {
      return getMetrics().density;
  }

  @CalledByNative
  public double getRefreshRate() {
      double result = getDisplay().getRefreshRate();
      // Sanity check.
      return (result >= 61 || result < 30) ? 0 : result;
  }

  @CalledByNative
  public String getNetworkCountryIso() {
      return mTelManager.getNetworkCountryIso();
  }

  private Display getDisplay() {
      return mWinManager.getDefaultDisplay();
  }

  private DisplayMetrics getMetrics() {
      DisplayMetrics metrics = new DisplayMetrics();
      getDisplay().getMetrics(metrics);
      return metrics;
  }

  @CalledByNative
  public static DeviceInfo create(Context context) {
      return new DeviceInfo(context);
  }
}
