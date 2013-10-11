// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <string>

#include "mojo/public/bindings/sample/generated/sample_service.h"
#include "mojo/public/bindings/sample/generated/sample_service_proxy.h"
#include "mojo/public/bindings/sample/generated/sample_service_stub.h"

namespace sample {

static void PrintSpacer(int depth) {
  for (int i = 0; i < depth; ++i)
    printf("   ");
}

static void Print(int depth, const char* name, bool value) {
  PrintSpacer(depth);
  printf("%s: %s\n", name, value ? "true" : "false");
}

static void Print(int depth, const char* name, int32_t value) {
  PrintSpacer(depth);
  printf("%s: %d\n", name, value);
}

static void Print(int depth, const char* name, uint8_t value) {
  PrintSpacer(depth);
  printf("%s: %u\n", name, value);
}

static void Print(int depth, const char* name, mojo::Handle value) {
  PrintSpacer(depth);
  printf("%s: 0x%x\n", name, value.value);
}

static void Print(int depth, const char* name, const mojo::String* str) {
  PrintSpacer(depth);
  printf("%s: \"%*s\"\n", name, static_cast<int>(str->size()), &str->at(0));
}

static void Print(int depth, const char* name, const Bar* bar) {
  PrintSpacer(depth);
  printf("%s: %p\n", name, bar);
  if (bar) {
    ++depth;
    Print(depth, "alpha", bar->alpha());
    Print(depth, "beta", bar->beta());
    Print(depth, "gamma", bar->gamma());
    --depth;
  }
}

template <typename T>
static void Print(int depth, const char* name, const mojo::Array<T>* array) {
  PrintSpacer(depth);
  printf("%s: %p\n", name, array);
  if (array) {
    ++depth;
    for (size_t i = 0; i < array->size(); ++i) {
      char buf[32];
      sprintf(buf, "%lu", static_cast<unsigned long>(i));
      Print(depth, buf, array->at(i));
    }
    --depth;
  }
}

static void Print(int depth, const char* name, const Foo* foo) {
  PrintSpacer(depth);
  printf("%s: %p\n", name, foo);
  if (foo) {
    ++depth;
    Print(depth, "name", foo->name());
    Print(depth, "x", foo->x());
    Print(depth, "y", foo->y());
    Print(depth, "a", foo->a());
    Print(depth, "b", foo->b());
    Print(depth, "c", foo->c());
    Print(depth, "bar", foo->bar());
    Print(depth, "extra_bars", foo->extra_bars());
    Print(depth, "data", foo->data());
    Print(depth, "files", foo->files());
    --depth;
  }
}

class ServiceImpl : public ServiceStub {
 public:
  virtual void Frobinate(const Foo* foo, bool baz, mojo::Handle port)
      MOJO_OVERRIDE {
    // Users code goes here to handle the incoming Frobinate message.
    // We'll just dump the Foo structure and all of its members.

    printf("Frobinate:\n");

    int depth = 1;
    Print(depth, "foo", foo);
    Print(depth, "baz", baz);
    Print(depth, "port", port);
  }
};

class SimpleMessageReceiver : public mojo::MessageReceiver {
 public:
  virtual bool Accept(mojo::Message* message) MOJO_OVERRIDE {
    // Imagine some IPC happened here.

    // In the receiving process, an implementation of ServiceStub is known to
    // the system. It receives the incoming message.
    ServiceImpl impl;

    ServiceStub* stub = &impl;
    return stub->Accept(message);
  }
};

void Exercise() {
  SimpleMessageReceiver receiver;

  // User has a proxy to a Service somehow.
  Service* service = new ServiceProxy(&receiver);

  // User constructs a message to send.

  // Notice that it doesn't matter in what order the structs / arrays are
  // allocated. Here, the various members of Foo are allocated before Foo is
  // allocated.

  mojo::ScratchBuffer buf;

  Bar* bar = Bar::New(&buf);
  bar->set_alpha(20);
  bar->set_beta(40);
  bar->set_gamma(60);

  const char kName[] = "foopy";
  mojo::String* name = mojo::String::NewCopyOf(&buf, std::string(kName));

  mojo::Array<Bar*>* extra_bars = mojo::Array<Bar*>::New(&buf, 3);
  for (size_t i = 0; i < extra_bars->size(); ++i) {
    Bar* bar = Bar::New(&buf);
    uint8_t base = static_cast<uint8_t>(i * 100);
    bar->set_alpha(base);
    bar->set_beta(base + 20);
    bar->set_gamma(base + 40);
    (*extra_bars)[i] = bar;
  }

  mojo::Array<uint8_t>* data = mojo::Array<uint8_t>::New(&buf, 10);
  for (size_t i = 0; i < data->size(); ++i)
    (*data)[i] = static_cast<uint8_t>(data->size() - i);

  mojo::Array<mojo::Handle>* files = mojo::Array<mojo::Handle>::New(&buf, 4);
  for (size_t i = 0; i < files->size(); ++i)
    (*files)[i].value = static_cast<MojoHandle>(0xFFFF - i);

  Foo* foo = Foo::New(&buf);
  foo->set_name(name);
  foo->set_x(1);
  foo->set_y(2);
  foo->set_a(false);
  foo->set_b(true);
  foo->set_c(false);
  foo->set_bar(bar);
  foo->set_extra_bars(extra_bars);
  foo->set_data(data);
  foo->set_files(files);

  mojo::Handle port = { 10 };

  service->Frobinate(foo, true, port);
}

}  // namespace sample

int main() {
  sample::Exercise();
  return 0;
}
