// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_IDIRECT3D9_MOCK_WIN_H_
#define CONTENT_GPU_GPU_IDIRECT3D9_MOCK_WIN_H_
#pragma once

#include <d3d9.h>
#include <windows.h>

#include "testing/gmock/include/gmock/gmock.h"

class IDirect3D9Mock : public IDirect3D9 {
 public:
  IDirect3D9Mock();
  virtual ~IDirect3D9Mock();

  MOCK_METHOD5_WITH_CALLTYPE(
      STDMETHODCALLTYPE, CheckDepthStencilMatch,
      HRESULT(UINT Adapter, D3DDEVTYPE DeviceType,
              D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat,
              D3DFORMAT DepthStencilFormat));
  MOCK_METHOD6_WITH_CALLTYPE(
      STDMETHODCALLTYPE, CheckDeviceFormat,
      HRESULT(UINT Adapter, D3DDEVTYPE DeviceType,
              D3DFORMAT AdapterFormat, DWORD Usage,
              D3DRESOURCETYPE RType, D3DFORMAT CheckFormat));
  MOCK_METHOD4_WITH_CALLTYPE(
      STDMETHODCALLTYPE, CheckDeviceFormatConversion,
      HRESULT(UINT Adapter, D3DDEVTYPE DeviceType,
              D3DFORMAT SourceFormat, D3DFORMAT TargetFormat));
  MOCK_METHOD6_WITH_CALLTYPE(
      STDMETHODCALLTYPE, CheckDeviceMultiSampleType,
      HRESULT(UINT Adapter, D3DDEVTYPE DeviceType,
              D3DFORMAT SurfaceFormat, BOOL Windowed,
              D3DMULTISAMPLE_TYPE MultiSampleType,
              DWORD* pQualityLevels));
  MOCK_METHOD5_WITH_CALLTYPE(
      STDMETHODCALLTYPE, CheckDeviceType,
      HRESULT(UINT Adapter, D3DDEVTYPE DevType,
              D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
              BOOL bWindowed));
  MOCK_METHOD6_WITH_CALLTYPE(
      STDMETHODCALLTYPE, CreateDevice,
      HRESULT(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
              DWORD BehaviorFlags,
              D3DPRESENT_PARAMETERS* pPresentationParameters,
              IDirect3DDevice9** ppReturnedDeviceInterface));
  MOCK_METHOD4_WITH_CALLTYPE(
      STDMETHODCALLTYPE, EnumAdapterModes,
      HRESULT(UINT Adapter, D3DFORMAT Format, UINT Mode,
              D3DDISPLAYMODE* pMode));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE, GetAdapterCount, UINT());
  MOCK_METHOD2_WITH_CALLTYPE(
      STDMETHODCALLTYPE, GetAdapterDisplayMode,
      HRESULT(UINT Adapter, D3DDISPLAYMODE* pMode));
  MOCK_METHOD3_WITH_CALLTYPE(
      STDMETHODCALLTYPE, GetAdapterIdentifier,
      HRESULT(UINT Adapter, DWORD Flags,
              D3DADAPTER_IDENTIFIER9* pIdentifier));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE, GetAdapterModeCount,
                             UINT(UINT Adapter, D3DFORMAT Format));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, GetAdapterMonitor,
                             HMONITOR(UINT Adapter));
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE, GetDeviceCaps,
                             HRESULT(UINT Adapter, D3DDEVTYPE DeviceType,
                                     D3DCAPS9* pCaps));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, RegisterSoftwareDevice,
                             HRESULT(void* pInitializeFunction));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE, QueryInterface,
                             HRESULT(REFIID riid, void** ppvObj));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE, AddRef, ULONG());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE, Release, ULONG());
};

#endif  // CONTENT_GPU_GPU_IDIRECT3D9_MOCK_WIN_H_
