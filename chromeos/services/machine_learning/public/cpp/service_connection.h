// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_
#define CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"

namespace chromeos {
namespace machine_learning {

// Encapsulates a connection to the Chrome OS ML Service daemon via its Mojo
// interface.
// Usage:
//   chromeos::machine_learning::mojom::ModelPtr model;
//   chromeos::machine_learning::mojom::ModelSpec spec = ...;
//   chromeos::machine_learning::ServiceConnection::GetInstance()
//       ->LoadModel(spec, mojom::MakeRequest(&model),
//                   base::BindOnce(&MyCallBack));
//   // Use |model| or wait for |MyCallBack|.
// Sequencing: Must be used on a single sequence (may be created on another).
class ServiceConnection {
 public:
  static ServiceConnection* GetInstance();

  // Instruct ML daemon to load the model specified in |spec|, binding a Model
  // implementation to |request|.
  // Bootstraps the initial Mojo connection to the daemon if necessary.
  void LoadModel(
      mojom::ModelSpecPtr spec,
      mojom::ModelRequest request,
      mojom::MachineLearningService::LoadModelCallback result_callback);

 private:
  friend class base::NoDestructor<ServiceConnection>;
  ServiceConnection();
  ~ServiceConnection();

  // Binds the top level interface |machine_learning_service_| to an
  // implementation in the ML Service daemon, if it is not already bound. The
  // binding is accomplished via D-Bus bootstrap.
  void BindMachineLearningServiceIfNeeded();

  // Mojo connection error handler. Resets |machine_learning_service_|, which
  // will be reconnected upon next use.
  void OnConnectionError();

  // Response callback for MlClient::BootstrapMojoConnection.
  void OnBootstrapMojoConnectionResponse(bool success);

  mojom::MachineLearningServicePtr machine_learning_service_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ServiceConnection);
};

}  // namespace machine_learning
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_
