// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <ostream>
#include <string>

#include "mojo/public/tests/simple_bindings_support.h"
#include "mojom/sample_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

template <>
class TypeConverter<sample::Bar, int32_t> {
 public:
  static int32_t ConvertTo(const sample::Bar& bar) {
    return static_cast<int32_t>(bar.alpha()) << 16 |
           static_cast<int32_t>(bar.beta()) << 8 |
           static_cast<int32_t>(bar.gamma());
  }
};

}  // namespace mojo

namespace sample {

// Set this variable to true to print the binary message in hex.
bool g_dump_message_as_hex = true;

// Make a sample |Foo|.
Foo MakeFoo() {
  mojo::String name("foopy");

  Bar::Builder bar;
  bar.set_alpha(20);
  bar.set_beta(40);
  bar.set_gamma(60);
  bar.set_type(BAR_VERTICAL);

  mojo::Array<Bar>::Builder extra_bars(3);
  for (size_t i = 0; i < extra_bars.size(); ++i) {
    BarType type = i % 2 == 0 ? BAR_VERTICAL : BAR_HORIZONTAL;
    Bar::Builder bar;
    uint8_t base = static_cast<uint8_t>(i * 100);
    bar.set_alpha(base);
    bar.set_beta(base + 20);
    bar.set_gamma(base + 40);
    bar.set_type(type);
    extra_bars[i] = bar.Finish();
  }

  mojo::Array<uint8_t>::Builder data(10);
  for (size_t i = 0; i < data.size(); ++i)
    data[i] = static_cast<uint8_t>(data.size() - i);

  mojo::Array<mojo::DataPipeConsumerHandle>::Builder input_streams(2);
  mojo::Array<mojo::DataPipeProducerHandle>::Builder output_streams(2);
  for (size_t i = 0; i < input_streams.size(); ++i) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 1024;
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    mojo::CreateDataPipe(&options, &producer, &consumer);
    input_streams[i] = consumer.Pass();
    output_streams[i] = producer.Pass();
  }

  mojo::ScopedMessagePipeHandle pipe0, pipe1;
  mojo::CreateMessagePipe(&pipe0, &pipe1);

  Foo::Builder foo;
  foo.set_name(name);
  foo.set_x(1);
  foo.set_y(2);
  foo.set_a(false);
  foo.set_b(true);
  foo.set_c(false);
  foo.set_bar(bar.Finish());
  foo.set_extra_bars(extra_bars.Finish());
  foo.set_data(data.Finish());
  foo.set_source(pipe1.Pass());
  foo.set_input_streams(input_streams.Finish());
  foo.set_output_streams(output_streams.Finish());

  return foo.Finish();
}

// Check that the given |Foo| is identical to the one made by |MakeFoo()|.
void CheckFoo(const Foo& foo) {
  const std::string kName("foopy");
  ASSERT_FALSE(foo.name().is_null());
  EXPECT_EQ(kName.size(), foo.name().size());
  for (size_t i = 0; i < std::min(kName.size(), foo.name().size()); i++) {
    // Test both |operator[]| and |at|.
    EXPECT_EQ(kName[i], foo.name().at(i)) << i;
    EXPECT_EQ(kName[i], foo.name()[i]) << i;
  }
  EXPECT_EQ(kName, foo.name().To<std::string>());

  EXPECT_EQ(1, foo.x());
  EXPECT_EQ(2, foo.y());
  EXPECT_FALSE(foo.a());
  EXPECT_TRUE(foo.b());
  EXPECT_FALSE(foo.c());

  EXPECT_EQ(20, foo.bar().alpha());
  EXPECT_EQ(40, foo.bar().beta());
  EXPECT_EQ(60, foo.bar().gamma());
  EXPECT_EQ(BAR_VERTICAL, foo.bar().type());

  EXPECT_EQ(3u, foo.extra_bars().size());
  for (size_t i = 0; i < foo.extra_bars().size(); i++) {
    uint8_t base = static_cast<uint8_t>(i * 100);
    BarType type = i % 2 == 0 ? BAR_VERTICAL : BAR_HORIZONTAL;
    EXPECT_EQ(base, foo.extra_bars()[i].alpha()) << i;
    EXPECT_EQ(base + 20, foo.extra_bars()[i].beta()) << i;
    EXPECT_EQ(base + 40, foo.extra_bars()[i].gamma()) << i;
    EXPECT_EQ(type, foo.extra_bars()[i].type()) << i;
  }

  EXPECT_EQ(10u, foo.data().size());
  for (size_t i = 0; i < foo.data().size(); ++i) {
    EXPECT_EQ(static_cast<uint8_t>(foo.data().size() - i), foo.data()[i]) << i;
  }

  EXPECT_FALSE(foo.input_streams().is_null());
  EXPECT_EQ(2u, foo.input_streams().size());

  EXPECT_FALSE(foo.output_streams().is_null());
  EXPECT_EQ(2u, foo.output_streams().size());
}

static void PrintSpacer(int depth) {
  for (int i = 0; i < depth; ++i)
    std::cout << "   ";
}

static void Print(int depth, const char* name, bool value) {
  PrintSpacer(depth);
  std::cout << name << ": " << (value ? "true" : "false") << std::endl;
}

static void Print(int depth, const char* name, int32_t value) {
  PrintSpacer(depth);
  std::cout << name << ": " << value << std::endl;
}

static void Print(int depth, const char* name, uint8_t value) {
  PrintSpacer(depth);
  std::cout << name << ": " << uint32_t(value) << std::endl;
}

static void Print(int depth, const char* name, mojo::Handle value) {
  PrintSpacer(depth);
  std::cout << name << ": 0x" << std::hex << value.value() << std::endl;
}

static void Print(int depth, const char* name, const mojo::String& str) {
  std::string s = str.To<std::string>();
  PrintSpacer(depth);
  std::cout << name << ": \"" << str.To<std::string>() << "\"" << std::endl;
}

static void Print(int depth, const char* name, const Bar& bar) {
  PrintSpacer(depth);
  std::cout << name << ":" << std::endl;
  if (!bar.is_null()) {
    ++depth;
    Print(depth, "alpha", bar.alpha());
    Print(depth, "beta", bar.beta());
    Print(depth, "gamma", bar.gamma());
    Print(depth, "packed", bar.To<int32_t>());
    --depth;
  }
}

template <typename T>
static void Print(int depth, const char* name,
                  const mojo::Passable<T>& passable) {
  Print(depth, name, passable.get());
}

template <typename T>
static void Print(int depth, const char* name, const mojo::Array<T>& array) {
  PrintSpacer(depth);
  std::cout << name << ":" << std::endl;
  if (!array.is_null()) {
    ++depth;
    for (size_t i = 0; i < array.size(); ++i) {
      std::stringstream buf;
      buf << i;
      Print(depth, buf.str().data(), array.at(i));
    }
    --depth;
  }
}

static void Print(int depth, const char* name, const Foo& foo) {
  PrintSpacer(depth);
  std::cout << name << ":" << std::endl;
  if (!foo.is_null()) {
    ++depth;
    Print(depth, "name", foo.name());
    Print(depth, "x", foo.x());
    Print(depth, "y", foo.y());
    Print(depth, "a", foo.a());
    Print(depth, "b", foo.b());
    Print(depth, "c", foo.c());
    Print(depth, "bar", foo.bar());
    Print(depth, "extra_bars", foo.extra_bars());
    Print(depth, "data", foo.data());
    Print(depth, "source", foo.source().get());
    Print(depth, "input_streams", foo.input_streams());
    Print(depth, "output_streams", foo.output_streams());
    --depth;
  }
}

static void DumpHex(const uint8_t* bytes, uint32_t num_bytes) {
  for (uint32_t i = 0; i < num_bytes; ++i) {
    std::cout << std::setw(2) << std::setfill('0') << std::hex <<
        uint32_t(bytes[i]);

    if (i % 16 == 15) {
      std::cout << std::endl;
      continue;
    }

    if (i % 2 == 1)
      std::cout << " ";
    if (i % 8 == 7)
      std::cout << " ";
  }
}

class ServiceImpl : public Service {
 public:
  virtual void Frobinate(const Foo& foo, int32_t baz,
                         mojo::ScopedMessagePipeHandle port)
      MOJO_OVERRIDE {
    // Users code goes here to handle the incoming Frobinate message.

    // We mainly check that we're given the expected arguments.
    CheckFoo(foo);
    EXPECT_EQ(BAZ_EXTRA, baz);

    // Also dump the Foo structure and all of its members.
    // TODO(vtl): Make it optional, so that the test spews less?
    std::cout << "Frobinate:" << std::endl;
    int depth = 1;
    Print(depth, "foo", foo);
    Print(depth, "baz", baz);
    Print(depth, "port", port.get());
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

    ServiceStub stub(&impl);
    return stub.Accept(message);
  }
};

TEST(BindingsSampleTest, Basic) {
  mojo::test::SimpleBindingsSupport bindings_support;
  SimpleMessageReceiver receiver;

  // User has a proxy to a Service somehow.
  Service* service = new ServiceProxy(&receiver);

  // User constructs a message to send.

  // Notice that it doesn't matter in what order the structs / arrays are
  // allocated. Here, the various members of Foo are allocated before Foo is
  // allocated.

  mojo::AllocationScope scope;

  Foo foo = MakeFoo();
  CheckFoo(foo);

  mojo::ScopedMessagePipeHandle port0, port1;
  mojo::CreateMessagePipe(&port0, &port1);

  service->Frobinate(foo, Service::BAZ_EXTRA, port0.Pass());
}

}  // namespace sample
