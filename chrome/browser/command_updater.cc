// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/command_updater.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/command_updater_delegate.h"

class CommandUpdater::Command {
 public:
  bool enabled;
  base::ObserverList<CommandObserver> observers;

  Command() : enabled(true) {}
};

CommandUpdater::CommandUpdater(CommandUpdaterDelegate* delegate)
    : delegate_(delegate) {
}

CommandUpdater::~CommandUpdater() {
}

bool CommandUpdater::SupportsCommand(int id) const {
  return commands_.find(id) != commands_.end();
}

bool CommandUpdater::IsCommandEnabled(int id) const {
  auto command = commands_.find(id);
  if (command == commands_.end())
    return false;
  return command->second->enabled;
}

bool CommandUpdater::ExecuteCommand(int id) {
  return ExecuteCommandWithDisposition(id, WindowOpenDisposition::CURRENT_TAB);
}

bool CommandUpdater::ExecuteCommandWithDisposition(
    int id,
    WindowOpenDisposition disposition) {
  if (SupportsCommand(id) && IsCommandEnabled(id)) {
    delegate_->ExecuteCommandWithDisposition(id, disposition);
    return true;
  }
  return false;
}

void CommandUpdater::AddCommandObserver(int id, CommandObserver* observer) {
  GetCommand(id, true)->observers.AddObserver(observer);
}

void CommandUpdater::RemoveCommandObserver(int id, CommandObserver* observer) {
  GetCommand(id, false)->observers.RemoveObserver(observer);
}

void CommandUpdater::RemoveCommandObserver(CommandObserver* observer) {
  for (const auto& command_pair : commands_) {
    Command* command = command_pair.second.get();
    if (command)
      command->observers.RemoveObserver(observer);
  }
}

void CommandUpdater::UpdateCommandEnabled(int id, bool enabled) {
  Command* command = GetCommand(id, true);
  if (command->enabled == enabled)
    return;  // Nothing to do.
  command->enabled = enabled;
  for (auto& observer : command->observers)
    observer.EnabledStateChangedForCommand(id, enabled);
}

CommandUpdater::Command* CommandUpdater::GetCommand(int id, bool create) {
  bool supported = SupportsCommand(id);
  if (supported)
    return commands_[id].get();

  DCHECK(create);
  std::unique_ptr<Command>& entry = commands_[id];
  entry = base::MakeUnique<Command>();
  return entry.get();
}
