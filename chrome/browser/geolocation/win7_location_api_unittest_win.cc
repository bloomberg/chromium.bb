// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Objbase.h>

#include <algorithm>
#include <cmath>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/common/geoposition.h"
#include "chrome/browser/geolocation/win7_location_api_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::DoDefault;
using testing::Invoke;
using testing::Return;

namespace {

class MockLatLongReport : public ILatLongReport {
 public:
  MockLatLongReport() : ref_count_(1) {
    ON_CALL(*this, GetAltitude(_))
        .WillByDefault(Invoke(this, &MockLatLongReport::GetAltitudeValid));
    ON_CALL(*this, GetAltitudeError(_))
        .WillByDefault(Invoke(this,
        &MockLatLongReport::GetAltitudeErrorValid));
    ON_CALL(*this, GetErrorRadius(_))
        .WillByDefault(Invoke(this, &MockLatLongReport::GetErrorRadiusValid));
    ON_CALL(*this, GetLatitude(_))
        .WillByDefault(Invoke(this, &MockLatLongReport::GetLatitudeValid));
    ON_CALL(*this, GetLongitude(_))
        .WillByDefault(Invoke(this, &MockLatLongReport::GetLongitudeValid));
    ON_CALL(*this, GetValue(_, _))
        .WillByDefault(Invoke(this, &MockLatLongReport::GetValueValid));
    ON_CALL(*this, Release())
        .WillByDefault(Invoke(this, &MockLatLongReport::ReleaseInternal));
    ON_CALL(*this, AddRef())
        .WillByDefault(Invoke(this, &MockLatLongReport::AddRefInternal));
  }

  // ILatLongReport
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetAltitude,
                             HRESULT(DOUBLE*));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetAltitudeError,
                             HRESULT(DOUBLE*));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetErrorRadius,
                             HRESULT(DOUBLE*));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetLatitude,
                             HRESULT(DOUBLE*));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetLongitude,
                             HRESULT(DOUBLE*));
  // ILocationReport
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetSensorID,
                             HRESULT(SENSOR_ID*));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTimestamp,
                             HRESULT(SYSTEMTIME*));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetValue,
                             HRESULT(REFPROPERTYKEY, PROPVARIANT*));
  // IUnknown
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             QueryInterface,
                             HRESULT(REFIID, void**));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             AddRef,
                             ULONG());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Release,
                             ULONG());

  HRESULT GetAltitudeValid(DOUBLE* altitude) {
    *altitude = 20.5;
    return S_OK;
  }
  HRESULT GetAltitudeErrorValid(DOUBLE* altitude_error) {
    *altitude_error = 10.0;
    return S_OK;
  }
  HRESULT GetErrorRadiusValid(DOUBLE* error) {
    *error = 5.0;
    return S_OK;
  }
  HRESULT GetLatitudeValid(DOUBLE* latitude) {
    *latitude = 51.0;
    return S_OK;
  }
  HRESULT GetLongitudeValid(DOUBLE* longitude) {
    *longitude = -0.1;
    return S_OK;
  }
  HRESULT GetValueValid(REFPROPERTYKEY prop_key, PROPVARIANT* prop) {
    prop->dblVal = 10.0;
    return S_OK;
  }

 private:
  ~MockLatLongReport() {}

  ULONG AddRefInternal() {
    return InterlockedIncrement(&ref_count_);
  }
  ULONG ReleaseInternal() {
    LONG new_ref_count = InterlockedDecrement(&ref_count_);
    if (0 == new_ref_count)
      delete this;
    return new_ref_count;
  }

  LONG ref_count_;
};

class MockReport : public ILocationReport {
 public:
  MockReport() : ref_count_(1) {
    mock_lat_long_report_ =
        new MockLatLongReport();
    ON_CALL(*this, QueryInterface(_, _))
        .WillByDefault(Invoke(this, &MockReport::QueryInterfaceValid));
    ON_CALL(*this, Release())
        .WillByDefault(Invoke(this, &MockReport::ReleaseInternal));
    ON_CALL(*this, AddRef())
        .WillByDefault(Invoke(this, &MockReport::AddRefInternal));
  }

  // ILocationReport
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetSensorID,
                             HRESULT(SENSOR_ID*));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTimestamp,
                             HRESULT(SYSTEMTIME*));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetValue,
                             HRESULT(REFPROPERTYKEY, PROPVARIANT*));
  // IUnknown
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             QueryInterface,
                             HRESULT(REFIID, void**));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             AddRef,
                             ULONG());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Release,
                             ULONG());

  MockLatLongReport* mock_lat_long_report_;

 private:
  ~MockReport() {
    mock_lat_long_report_->Release();
  }

  ULONG AddRefInternal() {
    return InterlockedIncrement(&ref_count_);
  }
  ULONG ReleaseInternal() {
    LONG new_ref_count = InterlockedDecrement(&ref_count_);
    if (0 == new_ref_count)
      delete this;
    return new_ref_count;
  }
  HRESULT QueryInterfaceValid(REFIID id, void** report) {
    EXPECT_TRUE(id == IID_ILatLongReport);
    *report = reinterpret_cast<ILatLongReport*>(mock_lat_long_report_);
    mock_lat_long_report_->AddRef();
    return S_OK;
  }

  LONG ref_count_;
};

class MockLocation : public ILocation {
 public:
  MockLocation() : ref_count_(1) {
    mock_report_ = new MockReport();
    ON_CALL(*this, SetDesiredAccuracy(_, _))
        .WillByDefault(Return(S_OK));
    ON_CALL(*this, GetReport(_, _))
        .WillByDefault(Invoke(this, &MockLocation::GetReportValid));
    ON_CALL(*this, RequestPermissions(_, _, _, _))
        .WillByDefault(Return(S_OK));
    ON_CALL(*this, AddRef())
        .WillByDefault(Invoke(this, &MockLocation::AddRefInternal));
    ON_CALL(*this, Release())
        .WillByDefault(Invoke(this, &MockLocation::ReleaseInternal));
  }

  // ILocation
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetDesiredAccuracy,
                             HRESULT(REFIID, LOCATION_DESIRED_ACCURACY*));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetReport,
                             HRESULT(REFIID, ILocationReport**));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetReportInterval,
                             HRESULT(REFIID, DWORD*));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetReportStatus,
                             HRESULT(REFIID, LOCATION_REPORT_STATUS*));
  MOCK_METHOD4_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             RequestPermissions,
                             HRESULT(HWND, IID*, ULONG, BOOL));
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             RegisterForReport,
                             HRESULT(ILocationEvents*, REFIID, DWORD));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SetDesiredAccuracy,
                             HRESULT(REFIID, LOCATION_DESIRED_ACCURACY));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SetReportInterval,
                             HRESULT(REFIID, DWORD));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             UnregisterForReport,
                             HRESULT(REFIID));
  // IUnknown
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             QueryInterface,
                             HRESULT(REFIID, void**));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             AddRef,
                             ULONG());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Release,
                             ULONG());

  MockReport* mock_report_;

 private:
  ~MockLocation() {
    mock_report_->Release();
  }

  HRESULT GetReportValid(REFIID report_type,
                         ILocationReport** location_report) {
    *location_report = reinterpret_cast<ILocationReport*>(mock_report_);
    mock_report_->AddRef();
    return S_OK;
  }
  ULONG AddRefInternal() {
    return InterlockedIncrement(&ref_count_);
  }
  ULONG ReleaseInternal() {
    LONG new_ref_count = InterlockedDecrement(&ref_count_);
    if (0 == new_ref_count)
      delete this;
    return new_ref_count;
  }

  LONG ref_count_;
};


HRESULT __stdcall MockPropVariantToDoubleFunction(REFPROPVARIANT propvarIn,
                                                  DOUBLE *pdblRet) {
  CHECK_EQ(10.0, propvarIn.dblVal);
  *pdblRet = 10.0;
  return S_OK;
}

// TODO(allanwoj): Either make mock classes into NiceMock classes
// or check every mock method call.
class GeolocationApiWin7Tests : public testing::Test {
 public:
  GeolocationApiWin7Tests() {
  }
  virtual void SetUp() {
    api_.reset(CreateMock());
    report_ = locator_->mock_report_;
    lat_long_report_ = report_->mock_lat_long_report_;
  }
  virtual void TearDown() {
    locator_->Release();
    api_.reset();
  }
  ~GeolocationApiWin7Tests() {
  }
 protected:
  Win7LocationApi* CreateMock() {
    MockLocation* locator = new MockLocation();
    locator_ = locator;
    return Win7LocationApi::CreateForTesting(&MockPropVariantToDoubleFunction,
                                             locator);
  }

  scoped_ptr<Win7LocationApi> api_;
  MockLatLongReport* lat_long_report_;
  MockLocation* locator_;
  MockReport* report_;
};

TEST_F(GeolocationApiWin7Tests, PermissionDenied) {
  EXPECT_CALL(*locator_, GetReport(_, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(E_ACCESSDENIED));
  Geoposition position;
  api_->GetPosition(&position);
  EXPECT_EQ(Geoposition::ERROR_CODE_PERMISSION_DENIED,
            position.error_code);
}

TEST_F(GeolocationApiWin7Tests, GetValidPosition) {
  EXPECT_CALL(*locator_, GetReport(_, _))
      .Times(AtLeast(1));
  Geoposition position;
  api_->GetPosition(&position);
  EXPECT_TRUE(position.IsValidFix());
}

TEST_F(GeolocationApiWin7Tests, GetInvalidPosition) {
  EXPECT_CALL(*lat_long_report_, GetLatitude(_))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(HRESULT_FROM_WIN32(ERROR_NO_DATA)));
  EXPECT_CALL(*locator_, GetReport(_, _))
      .Times(AtLeast(1));
  Geoposition position;
  api_->GetPosition(&position);
  EXPECT_FALSE(position.IsValidFix());
}

}  // namespace
