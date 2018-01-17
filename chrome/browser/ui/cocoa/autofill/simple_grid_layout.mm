// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/autofill/simple_grid_layout.h"

#include <stddef.h>

#include <algorithm>


namespace {
const int kAutoColumnIdStart = 1000000;  // Starting ID for autogeneration.
}

// Encapsulates state for a single NSView in the layout
class ViewState {
 public:
  ViewState(NSView* view, ColumnSet* column_set, int row, int column);

  // Gets the current width of the column associated with this view.
  float GetColumnWidth();

  // Get the preferred height for specified width.
  float GetHeightForWidth(float with);

  Column* GetColumn() const { return column_set_->GetColumn(column_); }

  int row_index()  { return row_; }
  NSView* view() { return view_; }
  float preferred_height() { return pref_height_; }
  void set_preferred_height(float height) { pref_height_ = height; }

 private:
  NSView* view_;
  ColumnSet* column_set_;
  int row_;
  int column_;
  float pref_height_;
};

class LayoutElement {
 public:
  LayoutElement(float resize_percent, int fixed_width);
  virtual ~LayoutElement() {}

  template <class T>
  static void ResetSizes(std::vector<std::unique_ptr<T>>* elements) {
    // Reset the layout width of each column.
    for (const auto& element : *elements)
      element->ResetSize();
  }

  template <class T>
  static void CalculateLocationsFromSize(
      std::vector<std::unique_ptr<T>>* elements) {
    // Reset the layout width of each column.
    int location = 0;
    for (const auto& element : *elements) {
      element->SetLocation(location);
      location += element->Size();
    }
  }

  float Size() { return size_; }

  void ResetSize() {
      size_ = fixed_width_;
  }

  void SetSize(float size) {
    size_ = size;
  }

  float Location() const {
    return location_;
  }

  // Adjusts the size of this LayoutElement to be the max of the current size
  // and the specified size.
  virtual void AdjustSize(float size) {
    size_ = std::max(size_, size);
  }

  void SetLocation(float location) {
    location_ = location;
  }

  bool IsResizable() {
    return resize_percent_ > 0.0f;
  }

  float ResizePercent() {
    return resize_percent_;
  }

 private:
  float resize_percent_;
  int fixed_width_;
  float size_;
  float location_;
};

LayoutElement::LayoutElement(float resize_percent, int fixed_width)
    : resize_percent_(resize_percent),
      fixed_width_(fixed_width),
      size_(0),
      location_(0) {
}

class Column : public LayoutElement {
 public:
   Column(float resize_percent, int fixed_width, bool is_padding);

   bool is_padding() { return is_padding_; }

  private:
    const bool is_padding_;
};

Column::Column(float resize_percent, int fixed_width, bool is_padding)
    : LayoutElement(resize_percent, fixed_width),
      is_padding_(is_padding) {
}

class Row : public LayoutElement {
 public:
  Row(float resize_percent, int fixed_height, ColumnSet* column_set);

  ColumnSet* column_set() { return column_set_; }

 private:
  ColumnSet* column_set_;
};

Row::Row(float resize_percent, int fixed_height, ColumnSet* column_set)
    : LayoutElement(resize_percent, fixed_height),
      column_set_(column_set) {
}

ViewState::ViewState(NSView* view, ColumnSet* column_set, int row, int column)
    : view_(view),
      column_set_(column_set),
      row_(row),
      column_(column) {}

float ViewState::GetColumnWidth() {
  return column_set_->GetColumnWidth(column_);
}

float ViewState::GetHeightForWidth(float width) {
  // NSView doesn't have any way to make height fit size, get frame height.
  return NSHeight([view_ frame]);
}

ColumnSet::ColumnSet(int id) : id_(id) {
}

ColumnSet::~ColumnSet() {
}

void ColumnSet::AddPaddingColumn(int fixed_width) {
  columns_.push_back(std::make_unique<Column>(0.0f, fixed_width, true));
}

void ColumnSet::AddColumn(float resize_percent) {
  columns_.push_back(std::make_unique<Column>(resize_percent, 0, false));
}

void ColumnSet::CalculateSize(float width) {
  // Reset column widths.
  LayoutElement::ResetSizes(&columns_);
  width = CalculateRemainingWidth(width);
  DistributeRemainingWidth(width);
}

void ColumnSet::ResetColumnXCoordinates() {
  LayoutElement::CalculateLocationsFromSize(&columns_);
}

float ColumnSet::CalculateRemainingWidth(float width) {
  for (const auto& column : columns_)
    width -= column->Size();

  return width;
}

void ColumnSet::DistributeRemainingWidth(float width) {
  float total_resize = 0.0f;
  int resizable_columns = 0.0;

  for (const auto& column : columns_) {
    if (column->IsResizable()) {
      total_resize += column->ResizePercent();
      resizable_columns++;
    }
  }

  float remaining_width = width;
  for (const auto& column : columns_) {
    if (column->IsResizable()) {
      float delta = (resizable_columns == 0)
                        ? remaining_width
                        : (width * column->ResizePercent() / total_resize);
      remaining_width -= delta;
      column->SetSize(column->Size() + delta);
      resizable_columns--;
    }
  }
}

float ColumnSet::GetColumnWidth(int column_index) {
  if (column_index < 0 || column_index >= num_columns())
    return 0.0;
  return columns_[column_index]->Size();
}

float ColumnSet::ColumnLocation(int column_index) {
  if (column_index < 0 || column_index >= num_columns())
    return 0.0;
  return columns_[column_index]->Location();
}

SimpleGridLayout::SimpleGridLayout(NSView* host)
    : next_column_(0),
      current_auto_id_(kAutoColumnIdStart),
      host_(host) {
  [host_ frame];
}

SimpleGridLayout::~SimpleGridLayout() {
}

ColumnSet* SimpleGridLayout::AddColumnSet(int id) {
  DCHECK(GetColumnSet(id) == nullptr);
  column_sets_.push_back(std::make_unique<ColumnSet>(id));
  return column_sets_.back().get();
}

ColumnSet* SimpleGridLayout::GetColumnSet(int id) {
  for (const auto& column_set : column_sets_) {
    if (column_set->id() == id) {
      return column_set.get();
    }
  }
  return nullptr;
}

void SimpleGridLayout::AddPaddingRow(int fixed_height) {
  AddRow(std::make_unique<Row>(0.0f, fixed_height, nullptr));
}

void SimpleGridLayout::StartRow(float vertical_resize, int column_set_id) {
  ColumnSet* column_set = GetColumnSet(column_set_id);
  DCHECK(column_set);
  AddRow(std::make_unique<Row>(vertical_resize, 0, column_set));
}

ColumnSet* SimpleGridLayout::AddRow() {
  AddRow(std::make_unique<Row>(0, 0, AddColumnSet(current_auto_id_++)));
  return column_sets_.back().get();
}

void SimpleGridLayout::SkipColumns(int col_count) {
  DCHECK(col_count > 0);
  next_column_ += col_count;
  ColumnSet* current_row_col_set_ = GetLastValidColumnSet();
  DCHECK(current_row_col_set_ &&
         next_column_ <= current_row_col_set_->num_columns());
  SkipPaddingColumns();
}

void SimpleGridLayout::AddView(NSView* view) {
  [host_ addSubview:view];
  DCHECK(next_column_ < GetLastValidColumnSet()->num_columns());
  view_states_.push_back(std::make_unique<ViewState>(
      view, GetLastValidColumnSet(), rows_.size() - 1, next_column_++));
  SkipPaddingColumns();
}

// Sizes elements to fit into the superViews bounds, according to constraints.
void SimpleGridLayout::Layout(NSView* superView) {
  SizeRowsAndColumns(NSWidth([superView bounds]));
  for (const auto& view_state : view_states_) {
    NSView* view = view_state->view();
    NSRect frame = NSMakeRect(view_state->GetColumn()->Location(),
                              rows_[view_state->row_index()]->Location(),
                              view_state->GetColumn()->Size(),
                              rows_[view_state->row_index()]->Size());
    [view setFrame:NSIntegralRect(frame)];
  }
}

void SimpleGridLayout::SizeRowsAndColumns(float width) {
  // Size all columns first.
  for (const auto& column_set : column_sets_) {
    column_set->CalculateSize(width);
    column_set->ResetColumnXCoordinates();
  }

  // Reset the height of each row.
  LayoutElement::ResetSizes(&rows_);

  // For each ViewState, obtain the preferred height
  for (const auto& view_state : view_states_) {
    // The view is resizable. As the pref height may vary with the width,
    // ask for the pref again.
    int actual_width = view_state->GetColumnWidth();

    // The width this view will get differs from its preferred. Some Views
    // pref height varies with its width; ask for the preferred again.
    view_state->set_preferred_height(
        view_state->GetHeightForWidth(actual_width));
  }


  // Make sure each row can accommodate all contained ViewStates.
  for (const auto& view_state : view_states_) {
    Row* row = rows_[view_state->row_index()].get();
    row->AdjustSize(view_state->preferred_height());
  }

  // Update the location of each of the rows.
  LayoutElement::CalculateLocationsFromSize(&rows_);
}

void SimpleGridLayout::SkipPaddingColumns() {
  ColumnSet* current_row_col_set_ = GetLastValidColumnSet();
  while (next_column_ < current_row_col_set_->num_columns() &&
         current_row_col_set_->GetColumn(next_column_)->is_padding()) {
    next_column_++;
  }
}

ColumnSet* SimpleGridLayout::GetLastValidColumnSet() {
  for (int i = num_rows() - 1; i >= 0; --i) {
    if (rows_[i]->column_set())
      return rows_[i]->column_set();
  }
  return nullptr;
}

float SimpleGridLayout::GetRowHeight(int row_index) {
  if (row_index < 0 || row_index >= num_rows())
    return 0.0;
  return rows_[row_index]->Size();
}

float SimpleGridLayout::GetRowLocation(int row_index) const {
  if (row_index < 0 || row_index >= num_rows())
    return 0.0;
  return rows_[row_index]->Location();
}

float SimpleGridLayout::GetPreferredHeightForWidth(float width) {
  if (rows_.empty())
    return 0.0f;

  SizeRowsAndColumns(width);
  return rows_.back()->Location() + rows_.back()->Size();
}

void SimpleGridLayout::AddRow(std::unique_ptr<Row> row) {
  next_column_ = 0;
  rows_.push_back(std::move(row));
}

