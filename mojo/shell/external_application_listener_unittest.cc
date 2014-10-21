// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "mojo/application_manager/application_manager.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/public/interfaces/application/application.mojom.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/shell/external_application_listener_posix.h"
#include "mojo/shell/external_application_registrar.mojom.h"
#include "mojo/shell/external_application_registrar_connection.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/unix_domain_client_socket_posix.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class ExternalApplicationListenerTest : public testing::Test {
 public:
  ExternalApplicationListenerTest() : io_thread_("io thread") {}
  virtual ~ExternalApplicationListenerTest() {}

  virtual void SetUp() override {
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);

    listener_.reset(new ExternalApplicationListenerPosix(
        loop_.task_runner(), io_thread_.task_runner()));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    socket_path_ = temp_dir_.path().Append(FILE_PATH_LITERAL("socket"));
  }

 protected:
  base::MessageLoop loop_;
  base::RunLoop run_loop_;
  base::Thread io_thread_;

  base::ScopedTempDir temp_dir_;
  ApplicationManager application_manager_;
  base::FilePath socket_path_;
  scoped_ptr<ExternalApplicationListener> listener_;
};

namespace {

class StubShellImpl : public InterfaceImpl<Shell> {
 private:
  void ConnectToApplication(
      const String& requestor_url,
      InterfaceRequest<ServiceProvider> in_service_provider) override {
    ServiceProviderPtr out_service_provider;
    out_service_provider.Bind(in_service_provider.PassMessagePipe());
    client()->AcceptConnection(requestor_url, out_service_provider.Pass());
  }
};

void DoLocalRegister(const GURL& app_url, ScopedMessagePipeHandle shell) {
  BindToPipe(new StubShellImpl, shell.Pass());
}

void QuitLoopOnConnect(scoped_refptr<base::TaskRunner> loop,
                       base::Closure quit_callback,
                       int rv) {
  EXPECT_EQ(net::OK, rv);
  loop->PostTask(FROM_HERE, quit_callback);
}

void ConnectOnIOThread(const base::FilePath& socket_path,
                       scoped_refptr<base::TaskRunner> to_quit,
                       base::Closure quit_callback) {
  ExternalApplicationRegistrarConnection connection(socket_path);
  connection.Connect(base::Bind(&QuitLoopOnConnect, to_quit, quit_callback));
}

}  // namespace

TEST_F(ExternalApplicationListenerTest, ConnectConnection) {
  listener_->ListenInBackground(socket_path_, base::Bind(&DoLocalRegister));
  listener_->WaitForListening();
  io_thread_.task_runner()->PostTask(FROM_HERE,
                                     base::Bind(&ConnectOnIOThread,
                                                socket_path_,
                                                loop_.task_runner(),
                                                run_loop_.QuitClosure()));
  run_loop_.Run();
}

namespace {

class QuitLoopOnConnectApplicationImpl : public InterfaceImpl<Application> {
 public:
  QuitLoopOnConnectApplicationImpl(const std::string& url,
                                   scoped_refptr<base::TaskRunner> loop,
                                   base::Closure quit_callback)
      : url_(url), to_quit_(loop), quit_callback_(quit_callback) {}

 private:
  void Initialize(Array<String> args) override {}

  void AcceptConnection(const String& requestor_url,
                        ServiceProviderPtr p) override {
    DVLOG(1) << url_ << " accepting connection from " << requestor_url;
    to_quit_->PostTask(FROM_HERE, quit_callback_);
  }

  const std::string url_;
  scoped_refptr<base::TaskRunner> to_quit_;
  base::Closure quit_callback_;
};

class FakeExternalApplication {
 public:
  FakeExternalApplication(const std::string& url) : url_(url) {}

  void ConnectSynchronously(const base::FilePath& socket_path) {
    connection_.reset(new ExternalApplicationRegistrarConnection(socket_path));
    net::TestCompletionCallback connect_callback;
    connection_->Connect(connect_callback.callback());
    connect_callback.WaitForResult();
  }

  // application_impl is the the actual implementation to be registered.
  void Register(scoped_ptr<InterfaceImpl<Application>> application_impl,
                base::Closure register_complete_callback) {
    connection_->Register(GURL(url_), &ptr_, register_complete_callback);
    application_impl_ = application_impl.Pass();
    ptr_.set_client(application_impl_.get());
  }

  void ConnectToAppByUrl(std::string app_url) {
    ServiceProviderPtr sp;
    ptr_->ConnectToApplication(app_url, GetProxy(&sp));
  }

  const std::string& url() { return url_; }

 private:
  const std::string url_;
  scoped_ptr<InterfaceImpl<Application>> application_impl_;
  ShellPtr ptr_;

  scoped_ptr<ExternalApplicationRegistrarConnection> connection_;
};

void ConnectToApp(FakeExternalApplication* connector,
                  FakeExternalApplication* connectee) {
  connector->ConnectToAppByUrl(connectee->url());
}

void NoOp() {
}

void ConnectAndRegisterOnIOThread(const base::FilePath& socket_path,
                                  scoped_refptr<base::TaskRunner> loop,
                                  base::Closure quit_callback,
                                  FakeExternalApplication* connector,
                                  FakeExternalApplication* connectee) {
  // Connect the first app to the registrar.
  connector->ConnectSynchronously(socket_path);
  // connector will use this implementation of the Mojo Application interface
  // once registration complete.
  scoped_ptr<QuitLoopOnConnectApplicationImpl> connector_app_impl(
      new QuitLoopOnConnectApplicationImpl(
          connector->url(), loop, quit_callback));
  // Since connectee won't be ready when connector is done registering, pass
  // in a do-nothing callback.
  connector->Register(connector_app_impl.Pass(), base::Bind(&NoOp));

  // Connect the second app to the registrar.
  connectee->ConnectSynchronously(socket_path);
  scoped_ptr<QuitLoopOnConnectApplicationImpl> connectee_app_impl(
      new QuitLoopOnConnectApplicationImpl(
          connectee->url(), loop, quit_callback));
  // After connectee is successfully registered, connector should be
  // able to connect to is by URL. Pass in a callback to attempt the
  // app -> app connection.
  connectee->Register(connectee_app_impl.Pass(),
                      base::Bind(&ConnectToApp, connector, connectee));
}

void DestroyOnIOThread(scoped_ptr<FakeExternalApplication> doomed1,
                       scoped_ptr<FakeExternalApplication> doomed2) {
}

}  // namespace

// Create two external applications, have them discover and connect to
// the registrar, and then have one app connect to the other by URL.
TEST_F(ExternalApplicationListenerTest, ConnectTwoExternalApplications) {
  listener_->ListenInBackground(
      socket_path_,
      base::Bind(&ApplicationManager::RegisterExternalApplication,
                 base::Unretained(&application_manager_)));
  listener_->WaitForListening();

  // Create two external apps.
  scoped_ptr<FakeExternalApplication> supersweet_app(
      new FakeExternalApplication("http://my.supersweet.app"));
  scoped_ptr<FakeExternalApplication> awesome_app(
      new FakeExternalApplication("http://my.awesome.app"));

  // Connecting and talking to the registrar has to happen on the IO thread.
  io_thread_.task_runner()->PostTask(FROM_HERE,
                                     base::Bind(&ConnectAndRegisterOnIOThread,
                                                socket_path_,
                                                loop_.task_runner(),
                                                run_loop_.QuitClosure(),
                                                supersweet_app.get(),
                                                awesome_app.get()));
  run_loop_.Run();

  // The apps need to be destroyed on the thread where they did socket stuff.
  io_thread_.task_runner()->PostTask(FROM_HERE,
                                     base::Bind(&DestroyOnIOThread,
                                                base::Passed(&supersweet_app),
                                                base::Passed(&awesome_app)));
}

}  // namespace shell
}  // namespace mojo
