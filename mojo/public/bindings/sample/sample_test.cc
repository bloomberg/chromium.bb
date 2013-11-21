// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>

#include "mojom/sample_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sample {

// Set this variable to true to print the binary message in hex.
bool g_dump_message_as_hex = true;

// Make a sample |Foo| in the given |ScratchBuffer|.
Foo* MakeFoo(mojo::ScratchBuffer* buf) {
  const std::string kName("foopy");
  mojo::String* name = mojo::String::NewCopyOf(buf, kName);

  Bar* bar = Bar::New(buf);
  bar->set_alpha(20);
  bar->set_beta(40);
  bar->set_gamma(60);

  mojo::Array<Bar*>* extra_bars = mojo::Array<Bar*>::New(buf, 3);
  for (size_t i = 0; i < extra_bars->size(); ++i) {
    Bar* bar = Bar::New(buf);
    uint8_t base = static_cast<uint8_t>(i * 100);
    bar->set_alpha(base);
    bar->set_beta(base + 20);
    bar->set_gamma(base + 40);
    (*extra_bars)[i] = bar;
  }

  mojo::Array<uint8_t>* data = mojo::Array<uint8_t>::New(buf, 10);
  for (size_t i = 0; i < data->size(); ++i)
    (*data)[i] = static_cast<uint8_t>(data->size() - i);

  mojo::Array<mojo::Handle>* files = mojo::Array<mojo::Handle>::New(buf, 4);
  for (size_t i = 0; i < files->size(); ++i)
    (*files)[i].set_value(static_cast<MojoHandle>(0xFFFF - i));

  Foo* foo = Foo::New(buf);
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

  return foo;
}

// Check that the given |Foo| is identical to the one made by |MakeFoo()|.
void CheckFoo(const Foo* foo) {
  const std::string kName("foopy");
  EXPECT_EQ(kName.size(), foo->name()->size());
  for (size_t i = 0; i < std::min(kName.size(), foo->name()->size()); i++) {
    // Test both |operator[]| and |at|.
    EXPECT_EQ(kName[i], foo->name()->at(i)) << i;
    EXPECT_EQ(kName[i], (*foo->name())[i]) << i;
  }
  EXPECT_EQ(kName, foo->name()->To<std::string>());

  EXPECT_EQ(1, foo->x());
  EXPECT_EQ(2, foo->y());
  EXPECT_FALSE(foo->a());
  EXPECT_TRUE(foo->b());
  EXPECT_FALSE(foo->c());

  EXPECT_EQ(20, foo->bar()->alpha());
  EXPECT_EQ(40, foo->bar()->beta());
  EXPECT_EQ(60, foo->bar()->gamma());

  EXPECT_EQ(3u, foo->extra_bars()->size());
  for (size_t i = 0; i < foo->extra_bars()->size(); i++) {
    uint8_t base = static_cast<uint8_t>(i * 100);
    EXPECT_EQ(base, (*foo->extra_bars())[i]->alpha()) << i;
    EXPECT_EQ(base + 20, (*foo->extra_bars())[i]->beta()) << i;
    EXPECT_EQ(base + 40, (*foo->extra_bars())[i]->gamma()) << i;
  }

  EXPECT_EQ(10u, foo->data()->size());
  for (size_t i = 0; i < foo->data()->size(); ++i) {
    EXPECT_EQ(static_cast<uint8_t>(foo->data()->size() - i),
              foo->data()->at(i)) << i;
  }

  EXPECT_EQ(4u, foo->files()->size());
  for (size_t i = 0; i < foo->files()->size(); ++i)
    EXPECT_EQ(static_cast<MojoHandle>(0xFFFF - i),
              foo->files()->at(i).value()) << i;
}


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
  printf("%s: 0x%x\n", name, value.value());
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

static void DumpHex(const uint8_t* bytes, uint32_t num_bytes) {
  for (uint32_t i = 0; i < num_bytes; ++i) {
    printf("%02x", bytes[i]);

    if (i % 16 == 15) {
      printf("\n");
      continue;
    }

    if (i % 2 == 1)
      printf(" ");
    if (i % 8 == 7)
      printf(" ");
  }
}

class ServiceImpl : public ServiceStub {
 public:
  virtual void Frobinate(const Foo* foo, bool baz, mojo::Handle port)
      MOJO_OVERRIDE {
    // Users code goes here to handle the incoming Frobinate message.

    // We mainly check that we're given the expected arguments.
    CheckFoo(foo);
    EXPECT_TRUE(baz);
    EXPECT_EQ(static_cast<MojoHandle>(10), port.value());

    // Also dump the Foo structure and all of its members.
    // TODO(vtl): Make it optional, so that the test spews less?
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

    if (g_dump_message_as_hex) {
      DumpHex(reinterpret_cast<const uint8_t*>(message->data),
              message->data->header.num_bytes);
    }

    // In the receiving process, an implementation of ServiceStub is known to
    // the system. It receives the incoming message.
    ServiceImpl impl;

    ServiceStub* stub = &impl;
    return stub->Accept(message);
  }
};

TEST(BindingsSampleTest, Basic) {
  SimpleMessageReceiver receiver;

  // User has a proxy to a Service somehow.
  Service* service = new ServiceProxy(&receiver);

  // User constructs a message to send.

  // Notice that it doesn't matter in what order the structs / arrays are
  // allocated. Here, the various members of Foo are allocated before Foo is
  // allocated.

  mojo::ScratchBuffer buf;
  Foo* foo = MakeFoo(&buf);
  CheckFoo(foo);

  mojo::Handle port(static_cast<MojoHandle>(10));

  service->Frobinate(foo, true, port);
}

}  // namespace sample
