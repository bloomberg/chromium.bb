// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/test/storage_area_test_util.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"

namespace content {
namespace test {
namespace {
void SuccessCallback(base::OnceClosure callback,
                     bool* success_out,
                     bool success) {
  *success_out = success;
  std::move(callback).Run();
}
}  // namespace

base::OnceCallback<void(bool)> MakeSuccessCallback(base::OnceClosure callback,
                                                   bool* success_out) {
  return base::BindOnce(&SuccessCallback, std::move(callback), success_out);
}

bool PutSync(blink::mojom::StorageArea* area,
             const std::vector<uint8_t>& key,
             const std::vector<uint8_t>& value,
             const base::Optional<std::vector<uint8_t>>& old_value,
             const std::string& source) {
  bool success = false;
  base::RunLoop loop;
  area->Put(key, value, old_value, source,
            base::BindLambdaForTesting([&](bool success_in) {
              success = success_in;
              loop.Quit();
            }));
  loop.Run();
  return success;
}

bool GetSync(blink::mojom::StorageArea* area,
             const std::vector<uint8_t>& key,
             std::vector<uint8_t>* data_out) {
  bool success = false;
  base::RunLoop loop;
  area->Get(key, base::BindLambdaForTesting(
                     [&](bool success_in, const std::vector<uint8_t>& value) {
                       success = success_in;
                       *data_out = std::move(value);
                       loop.Quit();
                     }));
  loop.Run();
  return success;
}

bool GetAllSync(blink::mojom::StorageArea* area,
                std::vector<blink::mojom::KeyValuePtr>* data_out) {
  DCHECK(data_out);
  base::RunLoop loop;
  bool complete = false;
  bool success = false;
  area->GetAll(
      GetAllCallback::CreateAndBind(&complete, loop.QuitClosure()),
      base::BindLambdaForTesting(
          [&](bool success_in, std::vector<blink::mojom::KeyValuePtr> data_in) {
            success = success_in;
            *data_out = std::move(data_in);
          }));
  loop.Run();
  DCHECK(complete);
  return success;
}

bool GetAllSyncOnDedicatedPipe(
    blink::mojom::StorageArea* area,
    std::vector<blink::mojom::KeyValuePtr>* data_out) {
  DCHECK(data_out);
  base::RunLoop loop;
  bool complete = false;
  bool success = false;
  area->GetAll(
      GetAllCallback::CreateAndBindOnDedicatedPipe(&complete,
                                                   loop.QuitClosure()),
      base::BindLambdaForTesting(
          [&](bool success_in, std::vector<blink::mojom::KeyValuePtr> data_in) {
            success = success_in;
            *data_out = std::move(data_in);
          }));
  loop.Run();
  DCHECK(complete);
  return success;
}

bool DeleteSync(blink::mojom::StorageArea* area,
                const std::vector<uint8_t>& key,
                const base::Optional<std::vector<uint8_t>>& client_old_value,
                const std::string& source) {
  bool success = false;
  base::RunLoop loop;
  area->Delete(key, client_old_value, source,
               base::BindLambdaForTesting([&](bool success_in) {
                 success = success_in;
                 loop.Quit();
               }));
  loop.Run();
  return success;
}

bool DeleteAllSync(blink::mojom::StorageArea* area, const std::string& source) {
  bool success = false;
  base::RunLoop loop;
  area->DeleteAll(source, base::BindLambdaForTesting([&](bool success_in) {
                    success = success_in;
                    loop.Quit();
                  }));
  loop.Run();
  return success;
}

base::OnceCallback<void(bool, std::vector<blink::mojom::KeyValuePtr>)>
MakeGetAllCallback(bool* success_out,
                   std::vector<blink::mojom::KeyValuePtr>* data_out) {
  DCHECK(success_out);
  DCHECK(data_out);
  return base::BindLambdaForTesting(
      [success_out, data_out](bool success_in,
                              std::vector<blink::mojom::KeyValuePtr> data_in) {
        *success_out = success_in;
        *data_out = std::move(data_in);
      });
}

// static
blink::mojom::StorageAreaGetAllCallbackAssociatedPtrInfo
GetAllCallback::CreateAndBind(bool* result, base::OnceClosure callback) {
  blink::mojom::StorageAreaGetAllCallbackAssociatedPtr ptr;
  auto request = mojo::MakeRequest(&ptr);
  mojo::MakeStrongAssociatedBinding(
      base::WrapUnique(new GetAllCallback(result, std::move(callback))),
      std::move(request));
  return ptr.PassInterface();
}

// static
blink::mojom::StorageAreaGetAllCallbackAssociatedPtrInfo
GetAllCallback::CreateAndBindOnDedicatedPipe(bool* result,
                                             base::OnceClosure callback) {
  blink::mojom::StorageAreaGetAllCallbackAssociatedPtr ptr;
  auto request = mojo::MakeRequestAssociatedWithDedicatedPipe(&ptr);
  mojo::MakeStrongAssociatedBinding(
      base::WrapUnique(new GetAllCallback(result, std::move(callback))),
      std::move(request));
  return ptr.PassInterface();
}

GetAllCallback::GetAllCallback(bool* result, base::OnceClosure callback)
    : result_(result), callback_(std::move(callback)) {}

GetAllCallback::~GetAllCallback() = default;

void GetAllCallback::Complete(bool success) {
  *result_ = success;
  if (callback_)
    std::move(callback_).Run();
}

MockLevelDBObserver::MockLevelDBObserver() : binding_(this) {}
MockLevelDBObserver::~MockLevelDBObserver() = default;

blink::mojom::StorageAreaObserverAssociatedPtrInfo MockLevelDBObserver::Bind() {
  blink::mojom::StorageAreaObserverAssociatedPtrInfo ptr_info;
  binding_.Bind(mojo::MakeRequest(&ptr_info));
  return ptr_info;
}

}  // namespace test
}  // namespace content
