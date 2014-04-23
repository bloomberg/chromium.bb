// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <cstdlib>
#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/test/utility/udp_proxy.h"

int main(int argc, char** argv) {
  if (argc < 5) {
    fprintf(stderr,
            "Usage: udp_proxy <localport> <remotehost> <remoteport> <type>\n"
            "Where type is one of: perfect, wifi, evil\n");
    exit(1);
  }

  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  InitLogging(logging::LoggingSettings());

  int local_port = atoi(argv[1]);
  int remote_port = atoi(argv[3]);
  net::IPAddressNumber remote_ip_number;
  net::IPAddressNumber local_ip_number;

  CHECK(net::ParseIPLiteralToNumber(argv[2], &remote_ip_number));
  CHECK(net::ParseIPLiteralToNumber("0.0.0.0", &local_ip_number));
  net::IPEndPoint remote_endpoint(remote_ip_number, remote_port);
  net::IPEndPoint local_endpoint(local_ip_number, local_port);

  scoped_ptr<media::cast::test::PacketPipe> in_pipe, out_pipe;
  std::string network_type = argv[4];
  if (network_type == "perfect") {
    // No action needed.
  } else if (network_type == "wifi") {
    in_pipe = media::cast::test::WifiNetwork().Pass();
    out_pipe = media::cast::test::WifiNetwork().Pass();
  } else if (network_type == "evil") {
    in_pipe = media::cast::test::EvilNetwork().Pass();
    out_pipe = media::cast::test::EvilNetwork().Pass();
  } else {
    fprintf(stderr, "Unknown network type.\n");
    exit(1);
  }

  printf("Press Ctrl-C when done.\n");
  scoped_ptr<media::cast::test::UDPProxy> proxy(
      media::cast::test::UDPProxy::Create(local_endpoint,
                                          remote_endpoint,
                                          in_pipe.Pass(),
                                          out_pipe.Pass(),
                                          NULL));
  base::MessageLoop().Run();  // Run forever.
  return 1;
}
