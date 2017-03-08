<!-- Copyright 2017 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file.
-->

# Media Router Integration and E2E Browser Tests

This directory contains the integration and end-to-end browser tests for Media
Router. Note that running these tests requires a dev build of the Media Router
component extension, which isn't open source. The project for upstreaming the
component extension is [tracked by this bug](http://crbug.com/698796).

## Test classes

* `MediaRouterIntegrationBrowserTest`: Tests that Media Router behaves as
specified by the Presentation API, and that its dialog is shown as expected.
Uses `mr.TestProvider` (provided the component extension) as the Media Route
Provider. Test cases that specifically test the functionalities of the Media
Router dialog are in `media_router_integration_ui_browsertest.cc`.

* `MediaRouterIntegrationIncognitoBrowserTest`: Same as
`MediaRouterIntegrationBrowserTest`, except for that the tests are run using an
incognito profile.

* `MediaRouterE2EBrowserTest`: Tests the casting functionality of Media Router
using the Cast Media Route Provider (provided by the component extension) and an
actual Chromecast device.
