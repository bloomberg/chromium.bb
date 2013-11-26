// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_H_
#define MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_H_

#include "mojo/public/bindings/lib/bindings.h"
#include "mojo/public/bindings/lib/message.h"
#include "mojom/sample_service_internal.h"

namespace sample {

class Bar {
 public:
  typedef internal::Bar_Data Data;

  bool is_null() const { return !data_; }  // XXX potential name conflict!

  uint8_t alpha() const { return data_->alpha(); }
  uint8_t beta() const { return data_->beta(); }
  uint8_t gamma() const { return data_->gamma(); }

  class Builder {
   public:
    explicit Builder(mojo::Buffer* buf);

    void set_alpha(uint8_t alpha) { data_->set_alpha(alpha); }
    void set_beta(uint8_t beta) { data_->set_beta(beta); }
    void set_gamma(uint8_t gamma) { data_->set_gamma(gamma); }

    Bar Finish();

   private:
    Bar::Data* data_;
  };

 private:
  friend class mojo::internal::WrapperHelper<Bar>;

  explicit Bar(const Data* data) : data_(data) {}
  const Data* data_;
};

class Foo {
 public:
  typedef internal::Foo_Data Data;

  bool is_null() const { return !data_; }

  int32_t x() const { return data_->x(); }
  int32_t y() const { return data_->y(); }
  bool a() const { return data_->a(); }
  bool b() const { return data_->b(); }
  bool c() const { return data_->c(); }
  const Bar bar() const { return mojo::internal::Wrap(data_->bar()); }
  const mojo::Array<uint8_t> data() const {
    return mojo::internal::Wrap(data_->data());
  }
  const mojo::Array<Bar> extra_bars() const {
    return mojo::internal::Wrap(data_->extra_bars());
  }
  const mojo::String name() const {
    return mojo::internal::Wrap(data_->name());
  }
  const mojo::Array<mojo::Handle> files() const {
    return mojo::internal::Wrap(data_->files());
  }

  class Builder {
   public:
    explicit Builder(mojo::Buffer* buf);

    void set_x(int32_t x) { data_->set_x(x); }
    void set_y(int32_t y) { data_->set_y(y); }
    void set_a(bool a) { data_->set_a(a); }
    void set_b(bool b) { data_->set_b(b); }
    void set_c(bool c) { data_->set_c(c); }
    void set_bar(const Bar& bar) {
      data_->set_bar(mojo::internal::Unwrap(bar));
    }
    void set_data(const mojo::Array<uint8_t>& data) {
      data_->set_data(mojo::internal::Unwrap(data));
    }
    void set_extra_bars(const mojo::Array<Bar>& extra_bars) {
      data_->set_extra_bars(mojo::internal::Unwrap(extra_bars));
    }
    void set_name(const mojo::String& name) {
      data_->set_name(mojo::internal::Unwrap(name));
    }
    void set_files(const mojo::Array<mojo::Handle>& files) {
      data_->set_files(mojo::internal::Unwrap(files));
    }

    Foo Finish();

   private:
    Foo::Data* data_;
  };

 private:
  friend class mojo::internal::WrapperHelper<Foo>;

  explicit Foo(const Data* data) : data_(data) {}
  const Data* data_;
};

class Service;
class ServiceProxy;
class ServiceStub;

class ServiceClient;
class ServiceClientProxy;
class ServiceClientStub;

class Service {
 public:
  typedef ServiceProxy _Proxy;
  typedef ServiceStub _Stub;
  typedef ServiceClient _Peer;

  virtual void Frobinate(const Foo& foo, bool baz, mojo::Handle port) = 0;
};

class ServiceClient {
 public:
  typedef ServiceClientProxy _Proxy;
  typedef ServiceClientStub _Stub;
  typedef Service _Peer;

  virtual void DidFrobinate(int32_t result)  = 0;
};

class ServiceProxy : public Service {
 public:
  explicit ServiceProxy(mojo::MessageReceiver* receiver);

  virtual void Frobinate(const Foo& Foo, bool baz, mojo::Handle port)
      MOJO_OVERRIDE;

 private:
  mojo::MessageReceiver* receiver_;
};

class ServiceClientProxy : public ServiceClient {
 public:
  explicit ServiceClientProxy(mojo::MessageReceiver* receiver);

  virtual void DidFrobinate(int32_t result) MOJO_OVERRIDE;

 private:
  mojo::MessageReceiver* receiver_;
};

class ServiceStub : public Service, public mojo::MessageReceiver {
 public:
  virtual bool Accept(mojo::Message* message) MOJO_OVERRIDE;
};

class ServiceClientStub : public ServiceClient, public mojo::MessageReceiver {
 public:
  virtual bool Accept(mojo::Message* message) MOJO_OVERRIDE;
};

}  // namespace sample

#endif  // MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_H_
