// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file deliberately has no header guard, as it's inlined in a number of
// files.

// Disables the QUIC protocol.
NETWORK_SWITCH(kDisableQuic, "disable-quic")

// Disables the HTTP/2 protocol.
NETWORK_SWITCH(kDisableHttp2, "disable-http2")

// Enables Alternate-Protocol when the port is user controlled (> 1024).
NETWORK_SWITCH(kEnableUserAlternateProtocolPorts,
               "enable-user-controlled-alternate-protocol-ports")

// Enables the QUIC protocol.  This is a temporary testing flag.
NETWORK_SWITCH(kEnableQuic, "enable-quic")

// Ignores certificate-related errors.
NETWORK_SWITCH(kIgnoreCertificateErrors, "ignore-certificate-errors")

// Specifies a comma separated list of host-port pairs to force use of QUIC on.
NETWORK_SWITCH(kOriginToForceQuicOn, "origin-to-force-quic-on")

// Specifies a comma separated list of QUIC connection options to send to
// the server.
NETWORK_SWITCH(kQuicConnectionOptions, "quic-connection-options")

// Specifies the maximum length for a QUIC packet.
NETWORK_SWITCH(kQuicMaxPacketLength, "quic-max-packet-length")

// Specifies the version of QUIC to use.
NETWORK_SWITCH(kQuicVersion, "quic-version")

// Allows for forcing socket connections to http/https to use fixed ports.
NETWORK_SWITCH(kTestingFixedHttpPort, "testing-fixed-http-port")
NETWORK_SWITCH(kTestingFixedHttpsPort, "testing-fixed-https-port")
