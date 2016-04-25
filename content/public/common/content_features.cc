// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/public/common/content_features.h"

namespace features {

// All features in alphabetical order.

// Enables brotli "Accept-Encoding" advertising and "Content-Encoding" support.
// Brotli format specification: http://www.ietf.org/id/draft-alakuijala-brotli
const base::Feature kBrotliEncoding{"brotli-encoding",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the credential management API:
// https://w3c.github.io/webappsec-credential-management/
const base::Feature kCredentialManagementAPI{"CredentialManagementAPI",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Downloads resumption will be controllable via a flag until it's enabled
// permanently. See https://crbug.com/7648
const base::Feature kDownloadResumption{"DownloadResumption",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Speculatively pre-evaluate Javascript which will likely use document.write to
// load an external script. The feature extracts the written markup and sends it
// to the preload scanner.
const base::Feature kDocumentWriteEvaluator{"DocumentWriteEvaluator",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// The Experimental Framework for controlling access to API experiments.
const base::Feature kExperimentalFramework{"ExperimentalFramework",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enable the material design playback UI for media elements.  This is always
// on for OS_ANDROID, but may be enabled by experiment for other platforms.
const base::Feature kNewMediaPlaybackUi{"NewMediaPlaybackUi",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// FeatureList definition for trials to enable the download button on
// MediaDocument.
const base::Feature kMediaDocumentDownloadButton{
    "MediaDocumentDownloadButton",
    base::FEATURE_DISABLED_BY_DEFAULT
};

// Non-validating reload on reload-to-refresh-content (e.g. pull-to-refresh).
// See https://crbug.com/558829
const base::Feature kNonValidatingReloadOnRefreshContent{
    "NonValidatingReloadOnRefreshContent",
    base::FEATURE_DISABLED_BY_DEFAULT};

// An experiment to optimize resource loading IPC for small resources.
// http://crbug.com/580928
const base::Feature kOptimizeIPCForSmallResource{
    "OptimizeForSmallResource",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Partial support for pointer event feature.
const base::Feature kPointerEvents{"PointerEvent",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Scrolls to compensate for layout movements (bit.ly/scroll-anchoring).
const base::Feature kScrollAnchoring{"ScrollAnchoring",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables implementation of the Cache-Control: stale-while-revalidate directive
// which permits servers to allow the use of stale resources while revalidation
// proceeds in the background. See http://crbug.com/348877
const base::Feature kStaleWhileRevalidate{"StaleWhileRevalidate2",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables token binding
// (https://www.ietf.org/id/draft-ietf-tokbind-protocol-04.txt).
const base::Feature kTokenBinding{"token-binding",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// An experimental User Agent Intervention on WebFonts loading.
const base::Feature kWebFontsInterventionV2{"WebFontsInterventionV2",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Makes WebRTC use ECDSA certs by default (i.e., when no cert type was
// specified in JS).
const base::Feature kWebRtcEcdsaDefault {"WebRTC-EnableWebRtcEcdsa",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the WebUSB API is enabled:
// https://wicg.github.io/webusb
const base::Feature kWebUsb{"WebUSB", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Use IME's own thread instead of using main UI thread. It also means that
// we will not use replica editor and do a round trip to renderer to synchronize
// with Blink data.
const base::Feature kImeThread{"ImeThread", base::FEATURE_DISABLED_BY_DEFAULT};

// FeatureList definition for the Seccomp field trial.
const base::Feature kSeccompSandboxAndroid{"SeccompSandboxAndroid",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace features
