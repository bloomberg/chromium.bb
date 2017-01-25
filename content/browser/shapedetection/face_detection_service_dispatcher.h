// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHAPEDETECTION_FACE_DETECTION_SERVICE_DISPATCHER_H_
#define CONTENT_BROWSER_SHAPEDETECTION_FACE_DETECTION_SERVICE_DISPATCHER_H_

#include "content/common/content_export.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/shape_detection/public/interfaces/constants.mojom.h"
#include "services/shape_detection/public/interfaces/facedetection_provider.mojom.h"

namespace content {

// Because the renderer cannot launch an out-of-process service on its own, we
// use |FaceDetectionServiceDispatcher| to forward requests to Service Manager,
// which then starts Face Detection Service in a utility process.
namespace FaceDetectionServiceDispatcher {

static void CreateMojoService(
    shape_detection::mojom::FaceDetectionProviderRequest request) {
  service_manager::Connector* connector =
      ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(shape_detection::mojom::kServiceName,
                           std::move(request));
}

}  // namespace FaceDetectionServiceDispatcher

}  // namespace content

#endif  // CONTENT_BROWSER_SHAPEDETECTION_FACE_DETECTION_SERVICE_DISPATCHER_H_
